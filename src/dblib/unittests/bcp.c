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

char lossstr[100];

static void getconnstr()
{
	char * colon;
	int i = 0;
	char conn_str[512];
	FILE *fp;
	fp = fopen("bcpconnectionstring.txt", "r");
	while (fscanf(fp, "%s", conn_str) != EOF) {}
	fprintf(stdout, "%s\n", conn_str);

	colon = strstr(conn_str, "SERVER");
	colon += 7;
	i = 0;
	while(*colon != ';')
	{
		server[i] = *colon;
		i++;
		colon ++;
	}
	colon = strstr(conn_str, "UID");
	colon += 4;
	i = 0;
	while(*colon != ';')
	{
		username[i] = *colon;
		i++;
		colon ++;
	}
	colon = strstr(conn_str, "PWD");
	colon += 4;
	i = 0;
	while(*colon != ';')
	{
		password[i] = *colon;
		i++;
		colon ++;
	}
	colon = strstr(conn_str, "DATABASE");
	colon += 9;
	i = 0;
	while(*colon != ';')
	{
		database[i] = *colon;
		i++;
		colon ++;
	}
	colon = strstr(conn_str, "PORT");
	colon += 5;
	i = 0;
	while(*colon != ';')
	{
		port[i] = *colon;
		i++;
		colon ++;
	}
	strcat(server,":");
	strcat(server, port);
}

static void getstreamstype()
{
	int stream_type;
	int readstream_type, sample_size;
	fread(&readstream_type, sizeof(readstream_type), 1, stdin);

	stream_type = readstream_type & fmstream_id;

	if (stream_type != fmstream_id)
	{
		fprintf(stdout, "Not a fm stream type\n");
		stream_type = readstream_type & gulstream_id;
		if (stream_type != gulstream_id)
		{
			fprintf(stdout, "Not a gul stream type either!\n");
			exit(-1);
		}
		else{fprintf(stdout, "is a GUL stream type\n"); data.type = 1;}		//THERE MUST BE A BETTER WAY. TODO.
	}
	else{fprintf(stdout, "is a FM stream type\n"); data.type = 0;}

	stream_type = streamno_mask & readstream_type;
	if (stream_type != 1 ) {
		fprintf(stdout, "Unsupported stream type: %d\n", stream_type);
		exit(-1);
	}
	fread(&sample_size, sizeof(sample_size), 1, stdin);
}

static int init(DBPROCESS * dbproc) 
{
	int res = 0;
	RETCODE rc;

	fprintf(stdout, "Dropping %s.%s..%s\n", server, database, data.table);
	snprintf(cmd, 512,"IF OBJECT_ID('%s','U') IS NOT NULL DROP TABLE %s", data.table, data.table);
	dbcmd(dbproc, cmd);
	dbsqlexec(dbproc);
	while ((rc=dbresults(dbproc)) == SUCCEED) {
		/* nop */
	}
	if (rc != NO_MORE_RESULTS)
		return 1;

	fprintf(stdout, "Creating %s.%s..%s\n", server, database, data.table);
	if (data.type == 0)
	{
		snprintf(cmd, 1024, "CREATE TABLE %s (EVENT_ID INT NOT NULL, PROG_ID INT NOT NULL,LAYER_ID INT NOT NULL,OUTPUT_ID INT NOT NULL, IDX INT NOT NULL,LOSS FLOAT NOT NULL)", data.table);
		dbcmd(dbproc, cmd);
	}
	if (data.type == 1)
	{
		snprintf(cmd, 1024, "CREATE TABLE %s (EVENT_ID INT NOT NULL, ITEM_ID INT NOT NULL, IDX INT NOT NULL,GUL FLOAT NOT NULL)", data.table);
		dbcmd(dbproc, cmd);	
	}

	if (dbsqlexec(dbproc) == FAIL) {
		res = 1;
	}
	while ((rc=dbresults(dbproc)) == SUCCEED) {
		dbprhead(dbproc);
		dbprrow(dbproc);
		while ((rc=dbnextrow(dbproc)) == REG_ROW) {
			dbprrow(dbproc);
		}
	}
	if (rc != NO_MORE_RESULTS)
		return 1;
	fprintf(stdout, "%s\n", res? "error" : "ok");
	return res;
}

#define VARCHAR_BIND(x) \
	bcp_bind( dbproc, (unsigned char *) &x, prefixlen, sizeof(x), NULL, termlen, SYBVARCHAR, col++ )

#define INT_BIND(x) \
	bcp_bind( dbproc, (unsigned char *) &x, prefixlen, -1, NULL, termlen, SYBINT4,    col++ )
 
static void bindFM (DBPROCESS * dbproc)
{
	enum { prefixlen = 0 };
	enum { termlen = 0 };
	enum NullValue { IsNull, IsNotNull };

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

	fOK = VARCHAR_BIND(lossstr);
	assert(fOK == SUCCEED);
}

static void bindGUL (DBPROCESS * dbproc)
{
	enum { prefixlen = 0 };
	enum { termlen = 0 };
	enum NullValue { IsNull, IsNotNull };

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

static void sendFM(DBPROCESS * dbproc)
{
	int i;
	i = fread(&p, sizeof(p), 1, stdin);
	while (i != 0) {
		i = fread(&q, sizeof(q), 1, stdin);
		while (i != 0) {
			if (q.sidx == 0) break;
			snprintf(lossstr, 100, "%f", q.loss);
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
			i = fread(&x, sizeof(x), 1, stdin);
			if (x.sidx == 0) break;
			snprintf(lossstr, 100, "%f", x.gul);
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
	int failed = 0;
	//get options (table name, process id.)
	int option = -1;
	char PID[100];
	while ((option = getopt(argc, argv, "t:p:")) != -1)
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
			printf("unknown parameter");
			exit(-1);
			break;
		}
	}

	freopen(NULL, "rb", stdin);
	getconnstr();
	getstreamstype();	

	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	login = dblogin();
	DBSETLUSER(login, username);
	DBSETLPWD(login, password);
	BCP_SETL(login, 1);

	fprintf(stdout, "About to open %s.%s\n", server, database);

	dbproc = dbopen(login, server);
	if (strlen(database))
		dbuse(dbproc, database);
	dbloginfree(login);

	if (init(dbproc))
		exit(1);

	/* set up and send the bcp */
	sprintf(cmd, "%s..%s", database, data.table);
	fprintf(stdout, "preparing to insert into %s ... ", cmd);
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
	
	rows_sent = bcp_batch(dbproc);
	if (rows_sent == -1) {
		fprintf(stdout, "batch failed\n");
	        exit(1);
	}

	fprintf(stdout, "OK\n");

	/* end bcp.  */
	if ((rows_sent += bcp_done(dbproc)) == -1)
	    printf("Bulk copy unsuccessful.\n");
	else
	    printf("%d rows copied.\n", rows_sent);


	printf("done\n");

	dbexit();

	failed = 0;

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
