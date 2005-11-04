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


#ifndef TESTCASE_H
#define TESTCASE_H


#include <wx/string.h>
#include <list>


namespace muleunit
{

class Test;
class TestResult;

typedef std::list<Test*> TestList;


/**
 * A TestCase is a collection of unit tests (instance of Test) and is
 * always specified by the first parameter of a Test declaration.
 */
class TestCase
{
public:

	/**
	 * Main TestCase constructor.
	 *
	 * @param name TestCase name
	 * @param testResult Pointer to the TestResult used to report results
	 * of executed Test
	 */
	TestCase(const wxString& name, TestResult *testResult);

	virtual ~TestCase();

	/**
	 * Add a Test to the Test list. This method is used by TestRegistry.
	 *
	 * @param test Test instance to add to the Test list.
	 */
	void addTest(Test *test);

	/**
	 * Get the Test list.
	 *
	 * @return Test list
	 */
	const TestList& getTests() const;

	/**
	 * Execute all Tests in the Test list of this TestCase.
	 */
	void run();

	/**
	 * Get the Test list size (number of Tests in this TestCase).
	 *
	 * @return The Test list size
	 */
	int getTestsCount() const;

	/**
	 * Get the total number of failures reported by all Tests.
	 *
	 * @return The total number of failures reported by all Tests. 0
	 * if no test were run or if no failures were reported.
	 */
	int getFailuresCount() const;

	/**
	 * Get the total number of successes reported by all Tests.
	 *
	 * @return The total number of successes reported by all Tests. 0
	 * if no test were run or if no successes were reported.
	 */
	int getSuccessesCount() const;

	/**
	 * Indicates whether or not this TestCase was executed.
	 *
	 * @return true if the method run() of this TestCase was called. false
	 * otherwise
	 */
	bool ran() const;

	/**
	 * Get the TestCase name. This name is specified by the first parameter
	 * of the Test declaration. For example, if a test was declared as
	 * TEST(TESTCASE1, TEST1), the TestCase name would be "TESTCASE1".
	 *
	 * @return The name of the TestCase
	 */
	const wxString& getName() const;

protected:
	int m_failuresCount;
	int m_successesCount;
	TestList m_tests;
	wxString m_name;
	TestResult* m_testResult;

private:
	void updateCount(Test *test);
	void runTests(Test *test);
	bool m_ran;
};

} // MuleUnit ns
#endif // TESTCASE_H

