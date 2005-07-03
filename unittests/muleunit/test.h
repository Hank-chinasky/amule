//
// MuleUnit: A minimalistic C++ Unit testing framework based on EasyUnit.
//
// Copyright (C) 2005 Mikkel Schubert (Xaignar@amule.org)
// Copyright (C) 2004 Barthelemy Dagenais (barthelemy@prologique.com)
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  

#ifndef TEST_H
#define TEST_H

#include <wx/string.h>
#include <list>

#include "testcase.h"
#include "testpartresult.h"


/**
 * MuleUnit namespace.
 * This is the namespace containing all muleunit classes.
 */
namespace muleunit
{

//! List used to store partial test-results
typedef std::list<TestPartResult*> TestPartResultList;


/**
 * Test class containing all macros to do unit testing. 
 * A test object represents a test that will be executed. Once it has been
 * executed, it reports all results in the testPartResult linked list.
 *
 * A failure occurs when a test fails (condition is false).
 * An error occurs when an exception is thrown during a test.
 * A success occurs if a test succeed (condition is true).
 */
class Test
{
public:
	/**
	 * Main Test constructor. Used to create a test that will register itself
	 * with TestRegistry and with its test case.
	 * @param testCaseName Name of the test case this test belongs to
	 * @param testName Name of this test
	 */
	Test(const wxString& testCaseName, const wxString& testName);

	/**
	 * Main Test desctructor
	 * Delete the testPartResult linked list. This is why the user should
	 * only use the macro provided by muleunit to report a test result.
	 */
	virtual ~Test();

	/**
	 * Fixtures that will be called after run().
	 */
	virtual void tearDown();

	/**
	 * Fixtures that will be called before run().
	 */
	virtual void setUp();

	/**
	 * Test code should be in this method.
	 * run() will be called by the Test's TestCase, hence subclasses of Test
	 * should override this method.
	 */
	virtual void run();

	/**
	 * Add a testpartresult to the testpartresult list of this test. 
	 * This method is used by the assertion macros to report success,
	 * failure or error.
	 *
	 * @param testPartResult The testpartresult to be added to the list
	 */
	virtual void addTestPartResult(TestPartResult *testPartResult);

	/**
	 * Get the testpartresult list of this test. If assertion macros
	 * and TEST and TESTF macros are used, there may be more than
	 * one successful testpartresult and no more than one error or failure.
	 *
	 * @return testPartResult The list of testpartresults of this test
	 */
	const TestPartResultList& getTestPartResult() const;

	/**
	 * Returns true if any errors or failures occured during the test.
	 */
	bool failed() const;
	
	/**
	 * Get the name of the TestCase this test belongs to. The name of the
	 * TestCase is the first parameter of the test declaration. For example,
	 * if a test is declared as TEST(TESTCASE1, TEST1), this method will return
	 * "TESTCASE1".
	 *
	 * @return The TestCase name of this test
	 */
	const wxString& getTestCaseName() const;

	/**
	 * Get the name of this test. The name of the test is the second
	 * parameter of the test declaration. For example,
	 * if a test is declared as TEST(TESTCASE1, TEST1), this method will return
	 * "TEST1".
	 *
	 * @return The name of this test.
	 */
	const wxString& getTestName() const;

protected:
	wxString m_testCaseName;
	wxString m_testName;
	TestCase* m_testCase;
	TestPartResultList m_testPartResults;
	bool m_failed;
};



/**
 * Helperfunction that converts basic types to strings.
 */
template <typename TYPE>
wxString StringFrom(const TYPE& value)
{
	return wxString() << value;
}



/*
 * Helper macros
 */
#define TO_STRING_EQUALS_F(expected,actual)\
  wxT("Expected : ") + StringFrom(expected) + wxT(" but Actual : ") + StringFrom(actual)

#define TO_STRING_EQUALS_S(expected,actual)\
  StringFrom(expected) + wxT(" == ") + StringFrom(actual)


/**
 * Asserts that a condition is true.
 * If the condition is not true, a failure is generated.
 * @param condition Condition to fullfill for the assertion to pass
 * @param message Message that will be displayed if this assertion fails
 */
#define ASSERT_TRUE_M(condition, message)\
	{ if (condition) {\
	addTestPartResult(new muleunit::TestPartResult(wxT(__FILE__),__LINE__,wxT(#condition),success));\
	} else {\
	addTestPartResult(new muleunit::TestPartResult(wxT(__FILE__),__LINE__, message,failure)); return;\
	}}

/**
 * Same as ASSERT_TRUE, but without an explicit message.
 */
#define ASSERT_TRUE(condition) \
	ASSERT_TRUE_M(condition, wxT(#condition));


/**
 * Same as ASSERT_TRUE, but without an explicit message and condition must be false.
 */
#define ASSERT_FALSE(condition) \
	ASSERT_TRUE_M(!(condition), wxT(#condition));


/**
 * Asserts that the two parameters are equals. Operator == must be defined.
 * If the two parameters are not equals, a failure is generated.
 * @param expected Expected value
 * @param actual Actual value to be compared
 * @param message Message that will be displayed if this assertion fails
 */
#define ASSERT_EQUALS_M(expected,actual,message)\
{ if (expected == actual) {\
	addTestPartResult(new muleunit::TestPartResult(wxT(__FILE__),__LINE__,wxT(#expected),success));\
	} else {\
	addTestPartResult(new muleunit::TestPartResult(wxT(__FILE__),__LINE__,message,failure)); return;\
	}}

/**
 * Same as ASSERT_EQUALS_M, but without an explicit message.
 */
#define ASSERT_EQUALS(expected,actual)\
	ASSERT_EQUALS_M(expected, actual, TO_STRING_EQUALS_S(expected,actual))


/**
 * Make a test fails with the given message.
 * @param text Failure message
 */
#define FAIL_M(text) \
	{ addTestPartResult(new TestPartResult(wxT(__FILE__), __LINE__,text,failure)); return; }


/**
 * Same as FAIL_M, but without an explicit message.
 */
#define FAIL() FAIL_M(wxT("Test failed."))


/**
 * Requires that an assertion of a certain type is raised.
 */
#define ASSERT_RAISES_M(type, call, message) \
	try { \
		call; \
		addTestPartResult(new muleunit::TestPartResult(wxT(__FILE__),__LINE__, message, failure)); \
		return; \
	} catch (type) { \
		addTestPartResult(new muleunit::TestPartResult(wxT(__FILE__),__LINE__, message, success)); \
	}


/**
 * Same as ASSERT_RAISES, but without an explicit message.
 */
#define ASSERT_RAISES(type, call) ASSERT_RAISES_M(type, (call), wxT("Exception of type " #type " not raised."))



/**
 * Define a test in a TestCase using test fixtures.
 * User should put his test code between brackets after using this macro.
 *
 * This macro should only be used if test fixtures were declared earlier in
 * this order: DECLARE, SETUP, TEARDOWN.
 * @param testCaseName TestCase name where the test belongs to. Should be
 * the same name of DECLARE, SETUP and TEARDOWN.
 * @param testName Unique test name.
 */
#define TEST(testCaseName, testName)                                           \
    class testCaseName##testName##Test : public testCaseName##Declare##Test    \
    {                                                                          \
    public:                                                                    \
        testCaseName##testName##Test()                                         \
            : testCaseName##Declare##Test(wxT(#testCaseName), wxT(#testName))  \
        {                                                                      \
        }                                                                      \
                                                                               \
        void run();                                                            \
    } testCaseName##testName##Instance;                                        \
                                                                               \
    void testCaseName##testName##Test::run()

/**
 * Location to declare variables and objets.
 * This is where user should declare members accessible by TESTF,
 * SETUP and TEARDOWN.
 *
 * User should not use brackets after using this macro. User should
 * not initialize any members here.
 *
 * @param testCaseName TestCase name of the fixtures
 * @see END_DECLARE for more information.
 */
#define DECLARE(testCaseName)\
	class testCaseName##Declare##Test : public Test \
	{ \
	public: \
		testCaseName##Declare##Test(const wxString& testCaseName, const wxString& testName) \
			: Test (testCaseName, testName) {} \
		virtual void run() = 0; \
		
/**
 * Ending macro used after DECLARE.
 *
 * User should use this macro after declaring members with
 * DECLARE macro.
 */
#define END_DECLARE };


/**
 * Macro for creating a fixture with no setup/teardown or member variables.
 */
#define DECLARE_SIMPLE(testCaseName) \
	DECLARE(testCaseName) \
	END_DECLARE;

} // MuleUnit ns
#endif // TEST_H

