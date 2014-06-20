//Copyright (c) 2014-2016 OKCoin
//Author : Chenzs
//2014/04/06

#ifndef OKCOIN_LOG_H
#define OKCOIN_LOG_H


#define OKCOIN_LOG
#define _MYSQL_DB_   //写到SQL数据库

#ifdef _MYSQL_DB_
#define LOG2DB 			1
#else
#define LOG2DB 			0
#endif 


#include <stdint.h>
#include <string>

#if LOG2DB
/*
  Include directly the different
  headers from cppconn/ and mysql_driver.h + mysql_util.h
  (and mysql_connection.h). This will reduce your build time!
*/
  // c++
//#include "mysql_conn_lib/include/mysql_driver.h"

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

/*
extern  "C"{
#include "mysql.h"
}*/
	
#else

/*
 Log to file
 */
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp> 
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>

#endif



bool OKCoin_Log_init();
bool OKCoin_Log_deInit();

bool OKCoin_Log_getTX(std::string hash, std::string fromIp, bool isCoinbase, int64_t valueOut);
bool OKCoin_Log_getBlk(std::string hash, std::string fromIp, unsigned long height, int64_t bc_time, unsigned long tx_count);


#endif
