# Gendata

 

Gendata downloads data from the server to the client.

 

## Linux installation:

### Prerequisites:

 

unixODBC and unixODBC-dev:

 

```
$ sudo apt-get install unixODBC unixODBC-dev
```

 

### SQL Server configuration:

 

you'll probably be using FreeTDS to connect to sqlserver (the microsoft driver has limited functionality):

 

```
$ sudo apt-get install tdsodbc
```

 

Edit /etc/odbcinst.ini to specify the location of the FreeTDS drivers.

these may change depending on your own configuration.

to find the location you can always run:

 

```
sudo updatedb && locate libtdsodbc.so
```

 

```
/etc/odbcinst.ini    
...

[ODBC]  
Trace = Yes  
TraceFile = /tmp/odbc.log  
[FreeTDS]  
Description = Free TDS driver   
Driver = "DRIVER LOCATION"   

...
```

 

### MySQL Server configuration:

```
$ sudo apt-get install libmyodbc
```

 

Edit /etc/odbcinst.ini to specify the location of the Mysql drivers.

these may change depending on your own congfiguration.

to find the location you can always run:

```
$ sudo updatedb && locate libmyodbc.so
```

 
```
/etc/odbcinst.ini:  
...  

[MySQL]  
Description = ODBC for MySQL  
Driver = "DRIVER LOCATION"  
FileUsage = 1  

...
```
you must configure the connection string in odbcconnect.txt.

comment out (hash) any connection strings you do not want the program to use.

For SQL Server, the driver specified must be FreeTDS:

 

```
1,DRIVER={FreeTDS};Server="SERVER_IP";Database=master;UID="USERNAME";PWD="PASSWORD";TDS_Version=8.0;Port="PORT";
```

 

For MYSQL Server, the driver specified must be MySQL:

```
2,DRIVER={MySQL};Server="SERVER_IP";DATABASE=mysql;Uid="USERNAME";PWD="PASSWORD";PORT="PORT";
```
Please note the numbers at the beginning of the string, this is important, number 1 indicates SQL Server configuration, number 2 indicates MYSQL Server.

### Recompilation

you should already have the necessary libraries needed to recompile the code should you need to:

```
g++ -std=c++11 gendataara.cpp -o gendataara -lodbc
```

this command is also in compile.sh, for convenience.

### Final notes

run.sh contains a sample line for running gendata, please do reconfigure your own. (./gendataara -h will give possible parameters)

 
