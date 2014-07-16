//Copyright (c) 2014-2016 OKCoin
//Author : Chenzs
//2014/04/06

#include "okcoin_log.h"
#include "util.h"
#include "script.h"
#include "base58.h"


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
static sql::PreparedStatement *pstmtOut;
static sql::PreparedStatement *pstmtUpdateTx;
static sql::PreparedStatement *pstmtEvent;
/*
extern  "C"{
static MYSQL  mMysql;
}
*/

#else
//static boost::once_flag debugPrintInitFlag = BOOST_ONCE_INIT;
static FILE* okcoinFileout = NULL;
static boost::mutex* mutexOkcoinLog = NULL;
#define  OKCOIN_LOG_FILENAME		"okcoin_tx.log"
#endif

static bool fInited = false;


bool Update_TxInfo(int64_t height, int64_t bindex, std::string hash){
#if LOG2DB
	/*
	 UpdateTransaction`(
	IN height bigint,
	IN bIndex bigint,
	IN txHash char(64)
)
*/
	if(!pstmtUpdateTx){
		pstmtUpdateTx = mysqlConn->prepareStatement("Call UpdateTransaction(?,?,?)");
	}
	assert(pstmtUpdateTx != NULL);
	try{
		pstmtUpdateTx->setInt64(1,height);
		pstmtUpdateTx->setInt64(2, bindex);
		pstmtUpdateTx->setString(3, hash);
		pstmtUpdateTx->executeUpdate();
	}catch(sql::SQLException &e){
		LogPrint("okcoin_log", "okcoin_log UpdateTransaction err %s \n", e.what());
		return false;
	}
#endif

	return true;
}



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

  	Driver *driver = get_driver_instance();
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
	assert(okcoinFileout == NULL);
    assert(mutexOkcoinLog == NULL);

    boost::filesystem::path pathDebug = GetDataDir() / OKCOIN_LOG_FILENAME;
    okcoinFileout = fopen(pathDebug.string().c_str(), "a");
    if (okcoinFileout) {
    	setbuf(okcoinFileout, NULL); // unbuffered
    	fInited = true;
    }
    mutexOkcoinLog = new boost::mutex();
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
		if(pstmtEvent)
			delete pstmtEvent;
		pstmtEvent = NULL;
	}
	/*
extern  "C"{
	mysql_close(&mMysql);
    mysql_server_end();
}
*/
#else
	if(okcoinFileout != NULL)
	{
		fclose(okcoinFileout);
		okcoinFileout = NULL;
	}
	
	if(mutexOkcoinLog != NULL){
		delete mutexOkcoinLog;
		mutexOkcoinLog = NULL;
	}
#endif

	fInited = false;
	LogPrint("okcoin_log", "OKCoin_Log_deInit\n");
	return true;
}


bool OKCoin_Log_getTxWhitOut(const CTransaction &tx, std::string fromIp,int64_t valueOut, int64_t valueIn, unsigned int sz){

	assert(fInited == true);

	int64_t txid = OKCoin_Log_getTX(tx.GetHash().ToString(), fromIp, tx.IsCoinBase(), valueOut,valueIn, sz, 
		tx.nVersion, tx.vout.size(), tx.vin.size());
	
	if(txid > 0){
#if LOG2DB
		if(!pstmtOut){
			//pstmtOut = mysqlConn->prepareStatement("CALL InsertVout(?,?,?,?,?)");
			pstmtOut = mysqlConn->prepareStatement("Call InsertOut_(?,?,?,?,?,?,?,?,?,?)");	
		}
		//INSERT into tb_vout(tx_id, n, v_out, to_addr) VALUES
		for (unsigned int i = 0; i < tx.vout.size(); i++)
		{
			const CTxOut& txout = tx.vout[i];
			txnouttype type;
    		std::vector<CTxDestination> addresses;
    		int nRequired;
    		if (ExtractDestinations(txout.scriptPubKey, type, addresses, nRequired)){
    			const CTxDestination& addr = addresses[0];
    			try{
    				/*

					pstmtOut->setInt(1, txid);
					pstmtOut->setInt(2,i);
					pstmtOut->setInt64(3, txout.nValue);
					pstmtOut->setString(4, CBitcoinAddress(addr).ToString());
					pstmtOut->setString(5,GetTxnOutputType(type));
					pstmtOut->executeUpdate();
					*/
    				/*
  IN n int(11),
IN nValue bigint(20),
IN addr varchar(45),
IN spent bit(1),
IN nType int(11),
IN script varchar(64),
IN addr_tag varchar(45),
IN addr_tag_link varchar(256),
IN tx_hash	varchar(64),
IN tx_index, bigInt
*/					
					pstmtOut->setInt(1, i);
					pstmtOut->setUInt64(2, txout.nValue);
					pstmtOut->setString(3, CBitcoinAddress(addr).ToString());
					pstmtOut->setBoolean(4, false);
					pstmtOut->setInt(5, type);
					pstmtOut->setString(6, txout.scriptPubKey.ToString());
					pstmtOut->setString(7, "");
					pstmtOut->setString(8, "");
					pstmtOut->setString(9, tx.GetHash().ToString());
					pstmtOut->setInt64(10, txid);
					pstmtOut->executeUpdate();

				}
				catch(sql::SQLException &e){
					LogPrint("okcoin_log", "okcoin_log Insert vout err %s \n", e.what());
				}

    		}
			
		}
#endif

		return true;
	}
	return false;
}

int OKCoin_Log_getTX(std::string hash, std::string fromIp, bool isCoinbase, int64_t valueOut, int64_t valueIn, unsigned int txSize, 
	int ver, int out_sz, int in_sz){

	assert(fInited == true);
	int ret = 0;
#if LOG2DB
	if(!fInited ){
		LogPrint("okcoin_log", "okcoin_log Insert to db fails, connection no inited \n");
		return false;
	}
	
	
	if(!pstmtTx){
		//pstmtTx =  mysqlConn->prepareStatement("Insert Into tb_tx(hash,bc_ip,is_coinbase,v_out,v_in,size,bc_time,bc_count) Values(?,?,?,?,?,?,date_add(now(),interval -8 hour),0)");
		//pstmtTx = mysqlConn->prepareStatement("CALL InsertTx(?,?,?,?,?,?,  @txid)");//tb_tx表
		//写入tb_transaction新表
		pstmtTx = mysqlConn->prepareStatement("CALL InsertTransaction_(?,?,?,?,?,?,?,?,?,?,?,?,@txid)");
/*PROCEDURE `InsertTransaction`(
IN hash char(64),
IN double_spend bit,
IN block_height bigint,
IN time datetime,
IN vout_sz int,
IN vin_sz int,
IN relayed_by varchar(45),
IN ver int,
IN size int,
IN block_index bigint,
IN vout bigint,
IN vin  bigint
)*/
		
	}
	
	assert(pstmtTx != NULL);
	try{/*
		pstmtTx->setString(1, hash);
		pstmtTx->setString(2, fromIp);
		pstmtTx->setBoolean(3,isCoinbase);
		pstmtTx->setInt64(4,valueOut);
		pstmtTx->setInt64(5,valueIn);
		pstmtTx->setUInt(6,txSize);
		pstmtTx->executeUpdate();
		
		 
		*/

    	pstmtTx->setString(1,hash);
    	pstmtTx->setBoolean(2,0);
    	pstmtTx->setInt64(3, 0);
    	pstmtTx->setDateTime(4, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()));
    	pstmtTx->setInt(5, out_sz);
    	pstmtTx->setInt(6, in_sz);
    	pstmtTx->setString(7, fromIp);
    	pstmtTx->setInt(8, ver);
    	pstmtTx->setUInt(9, txSize);
    	pstmtTx->setInt64(10, 0);
    	pstmtTx->setUInt64(11, valueOut);
    	pstmtTx->setUInt64(12, valueIn);
    	pstmtTx->executeUpdate();

    	std::auto_ptr<PreparedStatement> prepSelect(mysqlConn->prepareStatement("select @txid as tid"));  
    	std::auto_ptr<ResultSet>lpRes(prepSelect->executeQuery()); 
    	
    		int count = lpRes->rowsCount();
    		if(count > 0){
    			lpRes->first(); 
				ret = lpRes->getInt(1);  
			}
			lpRes->close();
		
		//pstmtTx->close();
       	LogPrint("okcoin_log", "okcoin inserttx id= %d\n",ret);
      	
	}catch(sql::SQLException &e){
		LogPrint("okcoin_log", "okcoin_log Insert tx err %s \n", e.what());
	}
	
	LogPrint("okcoin_log2db", "okcoin_log Insert tx result=%d, tx hash=%d timeoffset = %d\n", ret,hash, GetTimeOffset());
#else
	/*
	if (fileout == NULL)
            return false;
	boost::mutex::scoped_lock scoped_lock(*mutexOkcoinLog);
	int ret = fprintf(okcoinFileout, "time %s tx %s ip %s base %d out %lu \n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()).c_str(), hash.data(), fromIp.data()
		, isCoinbase, valueOut);
	*/
	ret = OKCoinLogPrint("type:add tx:%s ip:%s rt:%lu\n",  hash.data(), fromIp.data(), GetTime());

#endif
	return ret;
}

bool OKCoin_Log_getBlk(const CBlock &block, std::string fromIp, unsigned long height,unsigned int size, int64_t totalOut, int64_t totalIn){
	int ret = 0;
	assert(fInited == true);
#if LOG2DB
	if(!pstmtBlk){
		//pstmtBlk = mysqlConn->prepareStatement("Insert Into tb_blk(hash, bc_ip,height,tx_count,bc_time,size,v_out, v_in) Values(?,?,?,?,?,?,?,?)");
		//使用存储过程
		//pstmtBlk = mysqlConn->prepareStatement("CALL InsertBlk(?,?,?,?,?,?,?,?)");
		//新数据表
		pstmtBlk = mysqlConn->prepareStatement("CALL InsertBlock_(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,@blkIndex)");
	}
	assert(pstmtBlk != NULL);
	LogPrint("okcoin_log2db", "okcoin_log get blk");
	try{
		/*
		 * PROCEDURE `InsertBlock`(
		IN hash char(64),
		IN ver int,
		IN prev_block char(64),
		IN mrkl_root char(64),
		IN time timestamp,
		IN bits bigint,
		IN fee bigint,
		IN nonce bigint,
		IN n_tx int,
		IN size int,
		IN main_chain bit,
		IN height bigint,
		IN received_time timestamp,
		IN relayed_by varchar(45),
		IN vout bigint
		 */
		pstmtBlk->setString(1,block.GetHash().ToString());
		pstmtBlk->setInt(2, block.nVersion);
		pstmtBlk->setString(3, block.hashPrevBlock.ToString());
		pstmtBlk->setString(4, block.hashMerkleRoot.ToString());
		pstmtBlk->setDateTime(5, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", block.GetBlockTime()));
		pstmtBlk->setInt64(6, block.nBits);
		pstmtBlk->setInt64(7, totalIn - totalOut);
		pstmtBlk->setInt64(8, block.nNonce);
		pstmtBlk->setInt(9, block.vtx.size());
		pstmtBlk->setInt(10, size);
		pstmtBlk->setBoolean(11, 1);
		pstmtBlk->setInt64(12,height);
		pstmtBlk->setDateTime(13, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", block.GetBlockTime()));
		pstmtBlk->setString(14, fromIp);
		pstmtBlk->setUInt64(15, totalOut);
		pstmtBlk->executeUpdate();
		
		std::auto_ptr<PreparedStatement> prepSelect(mysqlConn->prepareStatement("select @blkIndex as bid"));  
    	std::auto_ptr<ResultSet>lpRes(prepSelect->executeQuery()); 
		int count = lpRes->rowsCount();
    	if(count > 0){
    		lpRes->first(); 
			ret = lpRes->getInt(1);  
		}
		lpRes->close();
		if(ret > 0){
			BOOST_FOREACH(const CTransaction& tx, block.vtx){
				Update_TxInfo(height, ret, tx.GetHash().ToString());
			}	
		}

	}catch(sql::SQLException &e){
		LogPrint("okcoin_log", "okcoin_log Insert blk err %s \n", e.what());
	}
	LogPrint("okcoin_log2db", "okcoin_log Insert blk result=%d, blk hash=%s \n", ret, block.GetHash().ToString());
#else
	ret = OKCoinLogPrint("type:add block:%s ip:%s rt:%lu\n",  block.GetHash().ToString().data(), fromIp.data(),GetTime);
#endif
	return ret > 0;
}

bool OKCoin_Log_getBlk(std::string hash, std::string fromIp, unsigned long height, int64_t bc_time, unsigned long tx_count, unsigned int blkSize,
	int64_t totalOut,int64_t totalIn){

	assert(fInited == true);
int ret = 0;
#if LOG2DB
	if(!pstmtBlk){
		//pstmtBlk = mysqlConn->prepareStatement("Insert Into tb_blk(hash, bc_ip,height,tx_count,bc_time,size,v_out, v_in) Values(?,?,?,?,?,?,?,?)");
		//使用存储过程
		pstmtBlk = mysqlConn->prepareStatement("CALL InsertBlk(?,?,?,?,?,?,?,?)");
		//新数据表
		//pstmtBlk = mysqlConn->prepareStatement("CALL InsertBlock(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
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
	LogPrint("okcoin_log2db", "okcoin_log Insert blk result=%d, blk hash=%s \n", ret,hash);
#else
	ret = OKCoinLogPrint("type:add block:%s ip:%s rt:%lu\n",  hash.data(), fromIp.data(), GetTime());
#endif

	return ret > 0;
}


/**
* type -- block:0 tx:1  
*/
int OKCoin_Log_Event(unsigned int type, unsigned int action,std::string hash, std::string fromip){
	assert(fInited == true);
	int ret;
#if LOG2DB
	if(pstmtEvent == NULL){
		/*`InsertEvent`(
	IN type smallint,
	IN action smallint,
	IN hashcode varchar(128),
	IN relayed	varchar(64),
	IN status	int)
	*/
		pstmtEvent = mysqlConn->prepareStatement("CALL InsertEvent(?,?,?,?,?)");
	}
	assert(pstmtEvent != NULL);
	try{
		pstmtEvent->setInt(1, type);
		pstmtEvent->setInt(2, action);
		pstmtEvent->setString(3, hash);
		pstmtEvent->setString(4, fromip);
		pstmtEvent->setInt(5, 0);
		ret = pstmtEvent->executeUpdate();
	}catch(sql::SQLException &e){
		LogPrint("okcoin_log", "okcoin_log Insert Event type=%d err %s \n", type, e.what());
	}


#else
	ret = OKCoinLogPrint("action:%d, type:%d block:%s ip:%s rt:%lu\n", action, type, hash.data(), fromip.data(), GetTime());
#endif
	LogPrint("okcoin_log", "okcoin_log Insert Event type=%d result= %s \n", type, ret);
	return ret;
}



#if !LOG2DB
int OKCoinLogPrintStr(const std::string &str)
{
	int ret = 0; // Returns total number of characters written
	
    if (okcoinFileout == NULL)
        return ret;

    boost::mutex::scoped_lock scoped_lock(*mutexOkcoinLog);
    ret += fprintf(okcoinFileout, "%s ", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()).c_str());
    ret = fwrite(str.data(), 1, str.size(), okcoinFileout);
    return ret;
}
#endif

