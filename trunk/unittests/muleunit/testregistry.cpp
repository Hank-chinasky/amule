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


#include "testregistry.h"
#include "defaulttestprinter.h"

using namespace muleunit;

TestRegistry::TestRegistry()
		: m_defaultPrinter(new DefaultTestPrinter())
{
}

TestRegistry::~TestRegistry()
{
	TestCaseList::iterator it = m_testCases.begin();
	for (; it != m_testCases.end(); ++it) {
		delete *it;
	}
		
	delete m_defaultPrinter;
}

void TestRegistry::addTest(Test *test)
{
	instance().add(test);
}

const TestResult* TestRegistry::runAndPrint()
{
	const TestResult *testResult = instance().runTests();
	instance().m_defaultPrinter->print(testResult);
	return testResult;
}


TestRegistry& TestRegistry::instance()
{
	static TestRegistry registry;
	return registry;
}

void TestRegistry::add(Test *test)
{
	const wxString tcName = test->getTestCaseName();
	const wxString tName = test->getTestName();

	
	if (m_testCases.empty() || m_testCases.back()->getName() != tcName) {
		m_testCases.push_back(new TestCase(tcName,&m_testResult));
	}

	m_testCases.back()->addTest(test);
}


const TestResult* TestRegistry::runTests()
{
	TestCaseList::iterator it = m_testCases.begin();
	for (; it != m_testCases.end(); ++it) {
		(*it)->run();
	}
	
	m_testResult.setTestCases(m_testCases);

	return &m_testResult;
}

