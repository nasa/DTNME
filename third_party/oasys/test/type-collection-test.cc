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
#  include <oasys-config.h>
#endif

#include "util/UnitTest.h"
#include "serialize/Serialize.h"
#include "serialize/TypeCollection.h"
#include "serialize/TypeShims.h"

using namespace oasys;

struct TestC {};

TYPE_COLLECTION_INSTANTIATE(TestC);

class Obj : public SerializableObject {
public:
    Obj(int id) : id_(id) {}
    virtual ~Obj();
    virtual const char* name() = 0;

    void serialize(SerializeAction*) {}
    
    int id_;
};
Obj::~Obj() {}

class Foo : public Obj {
public:
    static const int ID = 1;
    Foo(const Builder&) : Obj(ID) {}
    virtual const char* name() { return "foo"; }

private:
    Foo();
    Foo(const Foo&);
};

class Bar : public Obj {
public:
    static const int ID = 2;
    Bar(const Builder&) : Obj(ID) {}
    virtual const char* name() { return "bar"; }

private:
    Bar();
    Bar(const Foo&);
};

class Baz : public Obj {
public:
    static const int ID = 3;
    Baz(const Builder&) : Obj(ID) {}
    virtual const char* name() { return "baz"; }

private:
    Baz();
    Baz(const Baz&);
};

class MultipleInherit {
public:
    virtual int stub() { return 0; }
    virtual ~MultipleInherit() {}
};
 
class MultiLeaf : public MultipleInherit, 
                  public Obj {
public:
    static const int ID = 3;
    MultiLeaf(const Builder&) : Obj(ID) {}
    virtual const char* name() { return "MultiLeaf"; }

private:
    MultiLeaf();
    MultiLeaf(const MultiLeaf&);
};

TYPE_COLLECTION_DECLARE(TestC, Foo, 1);
TYPE_COLLECTION_DECLARE(TestC, Bar, 2);
TYPE_COLLECTION_DECLARE(TestC, MultiLeaf, 3);
TYPE_COLLECTION_DECLARE(TestC, Baz, 4);
TYPE_COLLECTION_GROUP(TestC, Obj, 1, 3);

TYPE_COLLECTION_DEFINE(TestC, Foo, 1);
TYPE_COLLECTION_DEFINE(TestC, Bar, 2);
TYPE_COLLECTION_DEFINE(TestC, MultiLeaf, 3);
TYPE_COLLECTION_DEFINE(TestC, Baz, 4);

DECLARE_TEST(TypeCollection1) {
    Foo* foo;

    CHECK(TypeCollectionInstance<TestC>::instance()->
          new_object(TypeCollectionCode<TestC, Foo>::TYPECODE, &foo) == 0);
    CHECK_EQUAL(foo->id_, Foo::ID);
    CHECK_EQUALSTR(foo->name(), "foo");
    CHECK(dynamic_cast<Foo*>(foo) != NULL);
    
    return UNIT_TEST_PASSED;
}


DECLARE_TEST(TypeCollection2) {
    Bar* bar;

    CHECK(TypeCollectionInstance<TestC>::instance()->
          new_object(TypeCollectionCode<TestC, Bar>::TYPECODE, &bar) == 0);
    CHECK(bar->id_ == Bar::ID);
    CHECK_EQUALSTR(bar->name(), "bar");
    CHECK(dynamic_cast<Bar*>(bar) != NULL);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TypeCollectionBogusCode) {
    Obj* obj = 0;
    CHECK(TypeCollectionInstance<TestC>::instance()->new_object(9999, &obj) == 
          TypeCollectionErr::TYPECODE);
    CHECK(obj == 0);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TypeCollectionMismatchCode) {
    Foo* foo = 0;
    CHECK(TypeCollectionInstance<TestC>::instance()->
          new_object(TypeCollectionCode<TestC, Bar>::TYPECODE, &foo) ==
          TypeCollectionErr::TYPECODE);
    CHECK(foo == 0);
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TypeCollectionGroup) {
    Obj* obj = 0;
    CHECK(TypeCollectionInstance<TestC>::instance()->
          new_object(TypeCollectionCode<TestC, Foo>::TYPECODE, &obj) == 0);
    
    CHECK(obj != 0);
    CHECK(obj->id_ == Foo::ID);
    CHECK_EQUALSTR(obj->name(), "foo");
    CHECK(dynamic_cast<Foo*>(obj) != NULL);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TypeCollectionNotInGroup) {
    Obj* obj = 0;
    CHECK(TypeCollectionInstance<TestC>::instance()->
          new_object(TypeCollectionCode<TestC, Baz>::TYPECODE, &obj) ==
          TypeCollectionErr::TYPECODE);
    CHECK(obj == 0);

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(TypeCollectionNames) {
    CHECK_EQUALSTR(TypeCollectionInstance<TestC>::instance()->type_name(1),
                   "TestC::Foo");
    CHECK_EQUALSTR(TypeCollectionInstance<TestC>::instance()->type_name(2),
                   "TestC::Bar");
    
    return UNIT_TEST_PASSED;
}

DECLARE_TEST(MultipleInheritance) {
    Obj* obj = 0;
    MultiLeaf* baz = reinterpret_cast<MultiLeaf*>(4);

    CHECK(TypeCollectionInstance<TestC>
            ::instance()->new_object(3, &obj) == 0);
    CHECK_EQUALSTR(obj->name(), "MultiLeaf");
    printf("obj %p multileaf %p multi %p\n", obj, 
           dynamic_cast<MultiLeaf*>(obj), 
           static_cast<MultipleInherit*>(dynamic_cast<MultiLeaf*>(obj)));
    CHECK(dynamic_cast<MultiLeaf*>(obj) != 0);
    CHECK(reinterpret_cast<void*>(dynamic_cast<MultiLeaf*>(obj)) != 
          reinterpret_cast<void*>(obj));
    CHECK( (reinterpret_cast<u_int64_t>(obj) - 
            reinterpret_cast<u_int64_t>(dynamic_cast<MultiLeaf*>(obj)) + 4)
            ==
           (reinterpret_cast<u_int64_t>(static_cast<Obj*>(baz))) );

    return UNIT_TEST_PASSED;
}

DECLARE_TESTER(TypeCollectionTest) {
    ADD_TEST(TypeCollection1);
    ADD_TEST(TypeCollection2);
    ADD_TEST(TypeCollectionBogusCode);
    ADD_TEST(TypeCollectionMismatchCode);
    ADD_TEST(TypeCollectionGroup);
    ADD_TEST(TypeCollectionNotInGroup);
    ADD_TEST(TypeCollectionNames);
    ADD_TEST(MultipleInheritance);
}

DECLARE_TEST_FILE(TypeCollectionTest, "builder test");
