#include "cppunit/Test.h"
#include "cppunit/TestSuite.h"
#include "StringTest.h"
#include "StringConstructionTest.h"
#include "StringCountCharTest.h"
#include "StringAssignTest.h"
#include "StringAppendTest.h"
#include "StringSubCopyTest.h"
#include "StringPrependTest.h"
#include "StringCaseTest.h"

CppUnit::Test *StringTestSuite()
{
	CppUnit::TestSuite *testSuite = new CppUnit::TestSuite();
	
	testSuite->addTest(StringConstructionTest::suite());
	testSuite->addTest(StringCountCharTest::suite());
	testSuite->addTest(StringAssignTest::suite());
	testSuite->addTest(StringAppendTest::suite());
	testSuite->addTest(StringSubCopyTest::suite());
	testSuite->addTest(StringPrependTest::suite());
	testSuite->addTest(StringCaseTest::suite());
	
	return(testSuite);
}
