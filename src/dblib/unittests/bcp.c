/* 
 * Purpose: Test bcp functions
 * Functions: bcp_batch bcp_bind bcp_done bcp_init bcp_sendrow 
 */

#include "common.h"
#include <assert.h>

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */

static void getconnstr(void);
static void getstreamstype(void);
static int init(DBPROCESS * dbproc);
static void bindFM(DBPROCESS * dbproc);
static void bindGUL(DBPROCESS * dbproc);

static void sendFM(DBPROCESS * dbproc);
static void sendGUL(DBPROCESS * dbproc);


static char cmd[1024];
const unsigned int gulstream_id = 1 << 24;
const unsigned int fmstream_id = 2 << 24;
const unsigned int streamno_mask = 0x00FFFFFF;

char server[20];
char username[512];
char password[512];
char database[512];
char port[20];

struct data {
	int type;
	char table[512];
}data;

struct fmlevelhdr {
	int event_id;
	int prog_id;
	int layer_id;
	int output_id;
}p;

struct fmlevelrec {
	int sidx;
	float loss;
}q;

struct gulSampleslevelHeader {
	int event_id;
	int item_id;
}z;

struct gulSampleslevelRec {
	int sidx;		// This has be stored for thresholds cannot be implied
	float gul;		// may want to cut down to singe this causes 4 byte padding for allignment
}x;

char lossstr[100];		//this is outside the structs because it would try to read into this string. We dont want this.

static void getconnstr()
{
	int colon;
	int count = 1;
	int i = 0;
	FILE *fp;
	fp = fopen("bcpconnectionstring.txt", "r");
	while((colon = fgetc(fp)) != EOF) {	
		fprintf(stdout, "%c", colon);
		if (colon != ';' && count == 1){server[i] = colon;}		//server;username;password;database;
		if (colon != ';' && count == 2){username[i] = colon;}
		if (colon != ';' && count == 3){password[i] = colon;}
		if (colon != ';' && count == 4){database[i] = colon;}
		i++;
		if (colon == ';'){count++; i = 0;}
    }
    fclose(fp);
}

static void getstreamstype()
{
	int stream_type;
	int readstream_type, sample_size;
	fread(&readstream_type, sizeof(readstream_type), 1, stdin);		//reads the first byte, and sees if it matches with the pre-specified "header" byte.

	stream_type = readstream_type & fmstream_id;	

	if (stream_type != fmstream_id)									//tries FM stream type, then tries GUL stream type.
	{
		fprintf(stdout, "Not a fm stream type\n");
		stream_type = readstream_type & gulstream_id;			//changes to GUL stream type after realisation of a not Fm stream type.
		if (stream_type != gulstream_id)
		{
			fprintf(stdout, "Not a gul stream type either!\n");
			exit(1);
		}
		else{fprintf(stdout, "is a GUL stream type\n"); data.type = 1;}		//THERE MUST BE A BETTER WAY. TODO.
	}
	else{fprintf(stdout, "is a FM stream type\n"); data.type = 0;}	//sets data.type, which is used in the rest of the program when handling the binary data.

	stream_type = streamno_mask & readstream_type;
	if (stream_type != 1 ) {
		fprintf(stdout, "Unsupported stream type: %d\n", stream_type);		//COPYPASTA from fmtocsv, dont exactly know what it does, and havent seen it working.
		exit(1);
	}
	fread(&sample_size, sizeof(sample_size), 1, stdin);		//for this application, the sample size is not needed, but we need to move another byte along in the stdin.
}

static int init(DBPROCESS * dbproc) 
{
	int res = 0;
	RETCODE rc;

	fprintf(stdout, "Dropping %s.%s..%s\n", server, database, data.table);
	snprintf(cmd, 512,"IF OBJECT_ID('%s','U') IS NOT NULL DROP TABLE %s", data.table, data.table);
	dbcmd(dbproc, cmd);
	dbsqlexec(dbproc);
	while ((rc=dbresults(dbproc)) == SUCCEED) {}		//drops any previous table if exists.
	if (rc != NO_MORE_RESULTS)							//not sure if necesarry.
		return 1;

	fprintf(stdout, "Creating %s.%s..%s\n", server, database, data.table);
	if (data.type == 0)
	{
		snprintf(cmd, 1024, "CREATE TABLE %s (EVENT_ID INT NOT NULL, PROG_ID INT NOT NULL,LAYER_ID INT NOT NULL,OUTPUT_ID INT NOT NULL, IDX INT NOT NULL,LOSS FLOAT NOT NULL)", data.table);
		dbcmd(dbproc, cmd);		//creates different tables depending on the stream type, i made the buffer large (1024) because it is user inputtable.
	}
	if (data.type == 1)
	{
		snprintf(cmd, 1024, "CREATE TABLE %s (EVENT_ID INT NOT NULL, ITEM_ID INT NOT NULL, IDX INT NOT NULL,GUL FLOAT NOT NULL)", data.table);
		dbcmd(dbproc, cmd);	
	}

	if (dbsqlexec(dbproc) == FAIL) {
		res = 1;
	}
	while ((rc=dbresults(dbproc)) == SUCCEED) {}
	if (rc != NO_MORE_RESULTS)
		return 1;
	fprintf(stdout, "%s\n", res? "error" : "ok");
	return res;
}

#define VARCHAR_BIND(x) \
	bcp_bind( dbproc, (unsigned char *) &x, 0, sizeof(x), NULL, 0, SYBVARCHAR, col++ )

#define INT_BIND(x) \
	bcp_bind( dbproc, (unsigned char *) &x, 0, -1, NULL, 0, SYBINT4,    col++ )		//some nice defines to reduce the code on the screen, 
 
static void bindFM (DBPROCESS * dbproc)				//BCP works by locking to a initialised variable, and sends data row by row, bcp_bind locks the variables to the BCP process.
{
	RETCODE fOK;
	int col=1;

	fOK = INT_BIND(p.event_id);
	assert(fOK == SUCCEED); 
	fOK = INT_BIND(p.prog_id);
	assert(fOK == SUCCEED); 
	fOK = INT_BIND(p.layer_id);
	assert(fOK == SUCCEED); 
	fOK = INT_BIND(p.output_id);
	assert(fOK == SUCCEED); 
	fOK = INT_BIND(q.sidx);
	assert(fOK == SUCCEED); 

	fOK = VARCHAR_BIND(lossstr);		// NB:/ i could not find a bcp_bind function for floating point numbers.  
	assert(fOK == SUCCEED);				// All the examples i found were using SYBVARCHAR variables...
}										// in this case we are not sending strings, but floats which have been turned into strings..

static void bindGUL (DBPROCESS * dbproc)	//different binds for FM or GUL.
{
	RETCODE fOK;
	int col=1;

	fOK = INT_BIND(z.event_id);
	assert(fOK == SUCCEED); 
	fOK = INT_BIND(z.item_id);
	assert(fOK == SUCCEED); 
	fOK = INT_BIND(x.sidx);
	assert(fOK == SUCCEED);  

	fOK = VARCHAR_BIND(lossstr);
	assert(fOK == SUCCEED);
}

static void sendFM(DBPROCESS * dbproc)	//reads the Fm binary and sends it row by row to  the server.
{
	int i;
	i = fread(&p, sizeof(p), 1, stdin);
	while (i != 0) {
		i = fread(&q, sizeof(q), 1, stdin);
		while (i != 0) {
			if (q.sidx == 0) break;
			snprintf(lossstr, 100, "%f", q.loss);	//turns float to string ready for sending. More info line 220
			if (bcp_sendrow(dbproc) == FAIL)
			{
	        	printf ("failed to send row\n");
	        	break;
		    }
			i = fread(&q, sizeof(q), 1, stdin);
		}
		if (i) i = fread(&p, sizeof(p), 1, stdin);
	}
}

static void sendGUL(DBPROCESS * dbproc)
{
	int i;
	while (i != 0)
	{
		i = fread(&z, sizeof(z), 1, stdin);
		while (i != 0)
		{
			i = fread(&x, sizeof(x), 1, stdin);		//different formats for the different binaries. Good Copypasta.
			if (x.sidx == 0) break;
			snprintf(lossstr, 100, "%f", x.gul);	//turns float to string ready for sending. More info line 220
			if (bcp_sendrow(dbproc) == FAIL)
			{
	        	printf ("failed to send row\n");
	        	break;
		    }
		}
	}
}

int main(int argc, char *argv[])
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int rows_sent = 0;
	int option = -1;
	char PID[100];
	while ((option = getopt(argc, argv, "t:p:")) != -1)		//get options (table name, process id.)
	{
		switch (option)
		{
			case 't':
			strcpy(data.table, strdup(optarg));
			break;
			case 'p':
			snprintf(PID, 100, "_%s", strdup(optarg));
			strcat(data.table, PID);
			break;
			default:
			printf("unknown parameter: -t is table name -p is ProcessID");
			exit(1);
			break;
		}
	}

	freopen(NULL, "rb", stdin);	//changes the input stream to "read binary."
	getconnstr();				//does what it says on the tin.
	getstreamstype();			//^^

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);
	dbinit();	
	login = dblogin();
	DBSETLUSER(login, username);
	DBSETLPWD(login, password);
	BCP_SETL(login, 1);			//required to enable BCP functionality

	fprintf(stdout, "About to open %s.%s\n", server, database);

	dbproc = dbopen(login, server);
	dbuse(dbproc, database);
	dbloginfree(login);

	if (init(dbproc))
		exit(1);

	sprintf(cmd, "%s..%s", database, data.table);
	fprintf(stdout, "preparing to insert into %s ... ", cmd);		//the difference between sprintf, fprintf, printf, and snprintf elude me, but it works.
	if (bcp_init(dbproc, cmd, NULL, NULL, DB_IN) == FAIL) 
	{
		fprintf(stdout, "failed\n");
    		exit(1);
	}
	fprintf(stdout, "OK\n");

	if (data.type == 0){bindFM(dbproc);}
	if (data.type == 1){bindGUL(dbproc);}	

	if (data.type == 0){sendFM(dbproc);}
	if (data.type == 1){sendGUL(dbproc);}
	
	rows_sent = bcp_batch(dbproc);		//finishes and closes BCP session
	if (rows_sent == -1) {
		fprintf(stdout, "batch failed\n");
	        exit(1);
	}

	fprintf(stdout, "OK\n");

	if ((rows_sent += bcp_done(dbproc)) == -1)
	    printf("Bulk copy unsuccessful.\n");
	else
	    printf("%d rows copied.\n", rows_sent);

	printf("done\n");

	dbexit();
	return 0;
}
