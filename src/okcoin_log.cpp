//Copyright (c) 2014-2016 OKCoin
//Author : Chenzs
//2014/04/06

#include "okcoin_log.h"
#include "util.h"

#if LOG2DB
#define DB_SERVER 		"127.0.0.1:3306"
#define DB_USER	 		"root"
#define DB_PASSWORD		"root"
#define DB_NAME			"blockchain"

using namespace sql;
using namespace sql::mysql;
static sql::Connection *mysqlConn;
static sql::PreparedStatement *pstmtTx;
static sql::PreparedStatement *pstmtBlk;
/*
extern  "C"{
static MYSQL  mMysql;
}
*/

#else
static boost::once_flag debugPrintInitFlag = BOOST_ONCE_INIT;
static FILE* fileout = NULL;
static boost::mutex* mutexDebugLog = NULL;
#endif

static bool fInited = false;

bool OKCoin_Log_init(){
	if(fInited == true){
		LogPrint("okcoin_log", "okcoin_log allready inited\n");
		return false;
	}
#if LOG2DB
	/* Create a connection */
	//load config
	std::string db_server= GetArg("-okdbhost", DB_SERVER);
	std::string db_user = GetArg("-okdbuser", DB_USER);
	std::string db_password = GetArg("-okdbpassword", DB_PASSWORD);
	std::string db_name= GetArg("-okdbname", DB_NAME);
	
	LogPrint("okcoin_log", "OKCoin_Log_init loadconfig ok_db_host = %s\n", db_server);

  	sql::Driver *driver = get_driver_instance();
  	mysqlConn = driver->connect(db_server,db_user, db_password);
  	fInited = mysqlConn? true: false;
  	assert(mysqlConn != NULL);
  	mysqlConn->setSchema(db_name);
	LogPrint("okcoin_log", "OKCoin_Log_init log2mysqldb result = %s \n", fInited ? "success":"fails");

	/*
extern  "C"{
	mysql_init(&mMysql);
	//connect   to   database
	mysql_real_connect(&mMysql,DB_SERVER,DB_USER,DB_PASSWORD,DB_NAME,3306,NULL,0);
	fInited = true;
}
*/
	
#else
	assert(fileout == NULL);
    assert(mutexDebugLog == NULL);

    boost::filesystem::path pathDebug = GetDataDir() / "okcoin_log_tx.log";
    fileout = fopen(pathDebug.string().c_str(), "a");
    if (fileout) {
    	setbuf(fileout, NULL); // unbuffered
    	fInited = true;
    }
    mutexDebugLog = new boost::mutex();
#endif
    return fInited;
}


bool OKCoin_Log_deInit(){
#if LOG2DB
	
	if(mysqlConn){
		mysqlConn->close();
		delete mysqlConn;
		mysqlConn = NULL;
		if(pstmtTx)
			delete pstmtTx;
		pstmtTx = NULL;
		if(pstmtBlk)
			delete pstmtBlk;
		pstmtBlk = NULL;
	}
	/*
extern  "C"{
	mysql_close(&mMysql);
    mysql_server_end();
}
*/
#else
	assert(fileout != NULL);
	fclose(fileout);
	fileout = NULL;
	assert(mutexDebugLog != NULL);
	delete mutexDebugLog;
	mutexDebugLog = NULL;
#endif

	fInited = false;
	LogPrint("okcoin_log", "OKCoin_Log_deInit\n");
	return true;
}


bool OKCoin_Log_getTX(std::string hash, std::string fromIp, bool isCoinbase, int64_t valueOut, int64_t valueIn, unsigned int txSize){
#if LOG2DB
	if(!fInited ){
		LogPrint("okcoin_log", "okcoin_log Insert to db fails, connection no inited \n");
		return false;
	}
	int ret = 0;
	
	if(!pstmtTx){
		//pstmtTx =  mysqlConn->prepareStatement("Insert Into tb_tx(hash,bc_ip,is_coinbase,v_out,v_in,size,bc_time,bc_count) Values(?,?,?,?,?,?,date_add(now(),interval -8 hour),0)");
		pstmtTx = mysqlConn->prepareStatement("CALL InsertTx(?,?,?,?,?,?, /*date_add(now(),interval -8 hour),0,*/ @txid)");
	}
	
	assert(pstmtTx != NULL);
	try{
		pstmtTx->setString(1, hash);
		pstmtTx->setString(2, fromIp);
		pstmtTx->setBoolean(3,isCoinbase);
		pstmtTx->setInt64(4,valueOut);
		pstmtTx->setInt64(5,valueIn);
		pstmtTx->setUInt(6,txSize);
		ret = pstmtTx->executeUpdate();

		std::auto_ptr<PreparedStatement> prepSelect(mysqlConn->prepareStatement("select @txid as tid"));  
    	std::auto_ptr<ResultSet>lpRes(prepSelect->executeQuery());  

    	while(lpRes->next())  
    	{  
       		 int ID = lpRes->getInt(1);  
       		 LogPrint("okcoin_log", "okcoin inserttx id= %d\n",ID);
       	}



	}catch(sql::SQLException &e){
		LogPrint("okcoin_log", "okcoin_log Insert tx err %s \n", e.what());
	}
	
	LogPrint("okcoin_log2db", "okcoin_log Insert tx result=%d, tx hash=%d \n", ret,hash);
#else
	if (fileout == NULL)
            return false;
	boost::mutex::scoped_lock scoped_lock(*mutexDebugLog);
	int ret = fprintf(fileout, "time %s tx %s ip %s base %d out %lu \n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()).c_str(), hash.data(), fromIp.data()
		, isCoinbase, valueOut);
	//fwrite(hash.data(), 1, str.size(), fileout);

#endif
	return ret > 0;
}

bool OKCoin_Log_getBlk(std::string hash, std::string fromIp, unsigned long height, int64_t bc_time, unsigned long tx_count, unsigned int blkSize,
	int64_t totalOut,int64_t totalIn){
int ret = 0;
#if LOG2DB
	if(!pstmtBlk){
		pstmtBlk = mysqlConn->prepareStatement("Insert Into tb_blk(hash, bc_ip,height,tx_count,bc_time,size,v_out, v_in) Values(?,?,?,?,?,?,?,?)");
		
	}
	assert(pstmtBlk != NULL);
	try{
		pstmtBlk->setString(1,hash);
		pstmtBlk->setString(2,fromIp);
		pstmtBlk->setUInt64(3,height);
		pstmtBlk->setUInt64(4,tx_count);
		pstmtBlk->setDateTime(5,DateTimeStrFormat("%Y-%m-%d %H:%M:%S", bc_time));
		pstmtBlk->setUInt(6,blkSize);
		pstmtBlk->setUInt64(7,totalOut);
		pstmtBlk->setUInt64(8, totalIn);
		ret = pstmtBlk->executeUpdate();


	}catch(sql::SQLException &e){
		LogPrint("okcoin_log", "okcoin_log Insert blk err %s \n", e.what());
	}
	LogPrint("okcoin_log2db", "okcoin_log Insert blk result=%d, blk hash=%d \n", ret,hash);
#else
	if (fileout == NULL)
            return false;
	boost::mutex::scoped_lock scoped_lock(*mutexDebugLog);
	ret = fprintf(fileout, "time %s blk %s ip %s height %lu count %lu \n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", bc_time).c_str(), hash.data(), fromIp.data()
		, height, tx_count);
#endif

	return ret > 0;
}

/*
static void DebugPrintInit()
{
   
}

*/

