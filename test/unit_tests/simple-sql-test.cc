/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <stdio.h>
#include <oasys/debug/Log.h>

#include "bundling/Bundle.h"

#include "storage/PostgresSQLImplementation.h"
#include "storage/MysqlSQLImplementation.h"

#include "storage/SQLBundleStore.h"




class foo : public oasys::SerializableObject, public oasys::Logger {
public:
  foo() : Logger ("/test/foo") {} ;
  virtual ~foo() {} ; 
  u_int16_t id ;
  u_int32_t  f1;
  u_int8_t f2 ; 
  bool b1;
  std::string  s1;
  void serialize(oasys::SerializeAction* a) ;

  void print() ; 
};

void
foo::print() {

    log_info("this object is %d char is %d  and id is %d \n",f1,f2,id);


}

void
foo::serialize(oasys::SerializeAction* a)
{
  a->process("id", &id);
  a->process("f1", &f1);
  a->process("f2", &f2);
  
}


void 
playsql(int i) {

    const char* database = "dtn";
    
    //  foo o1; o1.id = 771 ; o1.f1 = 123; o1.f2 = 'a'; foo o2;
    
    oasys::SQLImplementation *db ;
    
    if (i ==1) 
       db  =  new PostgresSQLImplementation();
    
    else
        db =  new MysqlSQLImplementation();

    db->connect(database);
           
    db->exec_query("drop table try;");
    
    BundleStore *bstore = new SQLBundleStore(db, "try");
    BundleStore::init(bstore);
    
    
    Bundle o1, o2;
    int id1 = 121;
    int id2 = 555;
   
    o1.source_.assign("bundles://internet/tcp://foo. Hello I'Am Sushant");
    o1.bundleid_ = id1;
    o1.custreq_ = 1;
    o1.fwd_rcpt_ = 1;
    bzero(o1.test_binary_,20);
    o1.test_binary_[0] = 2;
    o1.test_binary_[1] = 5;
    o1.test_binary_[2] = 38;
    o1.test_binary_[3] = o1.test_binary_[1] +     o1.test_binary_[2] ;
    
    o2.source_.assign("bundles://google/tcp://foo");
    o2.bundleid_ =  id2;
    o2.return_rcpt_ = 1;
    
    int retval2 = bstore->put(&o1,id1);
    retval2 = bstore->put(&o2,id2);
    
    Bundle *g1 = bstore->get(id1);
    Bundle *g2 = bstore->get(id2);
    
    retval2 = bstore->put(g1,id1);
    
    ASSERT(o1.bundleid_ == g1->bundleid_);
    ASSERT(o1.custreq_ == g1->custreq_);
    ASSERT(o1.custody_rcpt_ == g1->custody_rcpt_);
    ASSERT(o1.recv_rcpt_ == g1->recv_rcpt_);
    ASSERT(o1.fwd_rcpt_ == g1->fwd_rcpt_);
    ASSERT(o1.return_rcpt_ == g1->return_rcpt_);
    ASSERT(o1.test_binary_[2] == g1->test_binary_[2]);
   
    
    ASSERT (o2.bundleid_ == g2->bundleid_);
    
    
    
    db->close();
    
    exit(0);
    
}  

int
main(int argc, const char** argv)
{


    Log::init(LOG_DEBUG);
    playsql(atoi(argv[1]));
    Bundle b, b2;
    b.bundleid_ = 100;
    ASSERT(!b.source_.valid());
    ASSERT(!b2.source_.valid());
    
    b.source_.assign("bundles://internet/tcp://foo");
    ASSERT(b.source_.valid());
    ASSERT(b.source_.region().compare("internet") == 0);
    ASSERT(b.source_.admin().compare("tcp://foo") == 0);

}
