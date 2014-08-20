/**
 * mysql数据库连接池
 * 2014、04、20
 */

#include <stdexcept>
#include "mysql_connpool.h"
#include "config.h"
            
using namespace std;
extern Config *config;

ConnPool * ConnPool::connPool = NULL;

ConnPool::ConnPool(string host,string user,string password,int maxSize){
    connectionProperties["hostName"] = host;
    connectionProperties["userName"] = user;
    connectionProperties["password"] = password;
    connectionProperties["OPT_CONNECT_TIMEOUT"] = 600;
    connectionProperties["OPT_RECONNECT"] = true;
    
    this->maxSize = maxSize;
    this->lock = new Mutex();
    this->curSize = 0;
    //初始化driver
    try{
        this->driver = sql::mysql::get_driver_instance();  //这里不是线程安全的
    }
    catch(sql::SQLException &e){
        string errorMsg = string("SQLException: ") + e.what() + string(" MySQL error code: ") + int_to_string(e.getErrorCode()) + string(" SQLState ") +  e.getSQLState();
        Log::Write(__FILE__,__FUNCTION__,__LINE__,errorMsg);
    }
    catch(std::runtime_error &e){
        string errorMsg = string("runtime_error: ") + e.what(); 
        Log::Write(__FILE__,__FUNCTION__,__LINE__,errorMsg);
    }
    //初始化连接池
    this->Init(maxSize/2);
}

ConnPool::~ConnPool(){    
    this->Destroy();
    delete lock;
}

ConnPool *ConnPool::GetInstance(string host,string user,string password,int maxSize){
    if(connPool == NULL) {
        connPool = new ConnPool(host,user,password, maxSize);
    }
    
    return connPool;
}

void ConnPool::Init(int size){
    sql::Connection * conn ;
    lock->Lock();
    
    for(int i = 0; i < size ;){
        conn = this->CreateConnection();
        if(conn){
            i++;            
            conns.push_back(conn);
            ++curSize;
        }
        else{
            Log::Write(__FILE__,__FUNCTION__,__LINE__,"Init connpooo fail one");
        }
    }
    
    lock->UnLock();    
}

void ConnPool::Destroy(){
    deque<sql::Connection *>::iterator pos;
    
    lock->Lock();
    
    for(pos = conns.begin(); pos != conns.end();++pos){
        this->TerminateConnection(*pos);
    }
    
    curSize = 0;
    conns.clear();
    
    lock->UnLock();    
}

sql::Connection * ConnPool::CreateConnection(){//这里不负责curSize的增加
    sql::Connection *conn;
    
    try{
        conn = driver->connect(connectionProperties);
        Log::Write(__FILE__,__FUNCTION__,__LINE__,"create a mysql conn");
        return conn;
    }
    catch(sql::SQLException &e){
        string errorMsg = string("SQLException:") + e.what() + string(" MySQL error code: ") + int_to_string(e.getErrorCode()) + string(" SQLState ") +  e.getSQLState();
        Log::Write(__FILE__,__FUNCTION__,__LINE__,errorMsg);
        return NULL;
    }
    catch(std::runtime_error &e){
        string errorMsg = string("runtime_error: ") + e.what(); 
        Log::Write(__FILE__,__FUNCTION__,__LINE__,errorMsg);
        return NULL;
    }
}

void ConnPool::TerminateConnection(sql::Connection * conn){
    if(conn){
        try{
            conn->close();
        }
        catch(sql::SQLException &e){
            string errorMsg = string("SQLException:") + e.what() + string(" MySQL error code: ") + int_to_string(e.getErrorCode()) + string(" SQLState ") +  e.getSQLState();
            Log::Write(__FILE__,__FUNCTION__,__LINE__,errorMsg);
        }
        catch(std::runtime_error &e){
            string errorMsg = string("runtime_error: ") + e.what(); 
            Log::Write(__FILE__,__FUNCTION__,__LINE__,errorMsg);
        }
        
        delete conn;
    }
}

sql::Connection * ConnPool::GetConnection(){
    sql::Connection * conn;
    
    lock->Lock();
    
    if(conns.size() > 0){//有空闲连接,则返回
        conn = conns.front();
        conns.pop_front();
        
        if(conn->isClosed()){ //如果连接关闭,则重新打开一个连接
            Log::Write(__FILE__,__FUNCTION__,__LINE__,"a mysql conn has been closed");
            delete conn;
            conn = this->CreateConnection();
        }
        
        if(conn == NULL){ //创建连接不成功
            --curSize;
        }
        lock->UnLock();
        
        return conn;
    }
    else{
        if(curSize < maxSize){//还可以创建新的连接
            conn = this->CreateConnection();
            if(conn){
                ++curSize;
                lock->UnLock();
                return conn;
            }
            else{
                lock->UnLock();
                return NULL;
            }
        }
        else{//连接池已经满了
            lock->UnLock();
            return NULL;
        }
    }    
}

void ConnPool::ReleaseConnection(sql::Connection * conn){
    if(conn){
        lock->Lock();
        
        conns.push_back(conn);
        
        lock->UnLock();
    }
}

sql::Connection * ConnPool::GetConnectionTry(int maxNum){
    sql::Connection * conn;
    
    for(int i = 0; i < maxNum; ++i){
        conn = this->GetConnection();
        if(conn){
            return conn;
        }
        else {
            sleep(2);
        }
    }
    
    return NULL;
}
