//
// This file is part of the aMule Project.
//
// Copyright (c) 2006 Marcelo Roberto Jimenez ( phoenix@amule.org )
// Copyright (c) 2006 aMule Team ( admin@amule.org / http://www.amule.org )
//
// Any parts of this program derived from the xMule, lMule or eMule project,
// or contributed by third-party developers are copyrighted by their
// respective authors.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
//


// This define must not conflict with the one in the standard header
#ifndef AMULE_UPNP_H


#define AMULE_UPNP_H


#include <map>


#include "extern/upnp/upnp.h"


#include "Logger.h"				// for Add(Debug)LogLineM()


#include <libs/common/MuleDebug.h>
#include <libs/common/StringFunctions.h>


class CUPnPException : public CMuleException
{
public:
	CUPnPException(const wxString& desc)
	:
	CMuleException(wxT("CUPnPException"), desc) {}
};


class amuleIPV4Address;


class CDynamicLibHandle
{
private:
	wxString m_libname;
	void *const m_LibraryHandle;
	CDynamicLibHandle(const CDynamicLibHandle &);
	CDynamicLibHandle &operator=(const CDynamicLibHandle &);
	
public:
	CDynamicLibHandle(const char *libname);
	~CDynamicLibHandle();
	void *Get() const { return m_LibraryHandle; }
};


class CUPnPControlPoint;


class CUPnPLib
{
public:
	static const wxString &UPNP_ROOT_DEVICE;
	static const wxString &UPNP_DEVICE_IGW;
	static const wxString &UPNP_DEVICE_WAN;
	static const wxString &UPNP_DEVICE_WAN_CONNECTION;
	static const wxString &UPNP_DEVICE_LAN;
	static const wxString &UPNP_SERVICE_LAYER3_FORWARDING;
	static const wxString &UPNP_SERVICE_WAN_COMMON_INTERFACE_CONFIG;
	static const wxString &UPNP_SERVICE_WAN_IP_CONNECTION;
	static const wxString &UPNP_SERVICE_WAN_PPP_CONNECTION;
	CUPnPControlPoint &m_ctrlPoint;
	
private:
	// dlopen stuff
	static const int NUM_LIB_IXML_SYMBOLS = 8;
	static const char *s_LibIXMLSymbols[NUM_LIB_IXML_SYMBOLS];
	static const int NUM_LIB_UPNP_SYMBOLS = 17;
	static const char *s_LibUPnPSymbols[NUM_LIB_UPNP_SYMBOLS];
	CDynamicLibHandle m_LibIXMLHandle;
	CDynamicLibHandle m_LibUPnPHandle;
	
public:
	CUPnPLib(CUPnPControlPoint &ctrlPoint);
	~CUPnPLib() {}

	// Convenience function so we don't have to write explicit calls 
	// to char2unicode every time
	Char2UnicodeBuf GetUPnPErrorMessage(int code) const;
	
	// Convenience function to avoid repetitive processing of error
	// messages
	wxString processUPnPErrorMessage(
		const wxString &messsage,
		int code,
		IXML_Document *doc) const;

	// Processing response to actions
	void ProcessActionResponse(
		IXML_Document *RespDoc,
		const wxString &actionName) const;
	
	// IXML_Element
	IXML_Element *Element_GetRootElement(
		IXML_Document *doc) const;
	IXML_Element *Element_GetFirstChild(
		IXML_Element *parent) const;
	IXML_Element *Element_GetNextSibling(
		IXML_Element *child) const;
	const DOMString Element_GetTag(
		IXML_Element *element) const;
	const wxString Element_GetTextValue(
		IXML_Element *element) const;
	const wxString Element_GetChildValueByTag(
		IXML_Element *element,
		const DOMString tag) const;
	IXML_Element *Element_GetFirstChildByTag(
		IXML_Element *element,
		const DOMString tag) const;
	IXML_Element *Element_GetNextSiblingByTag(
		IXML_Element *element,
		const DOMString tag) const;
	const wxString Element_GetAttributeByTag(
		IXML_Element *element,
		const DOMString tag) const;
	
	// ixml api
	IXML_Node *(*m_ixmlNode_getFirstChild)(IXML_Node *nodeptr);
	IXML_Node *(*m_ixmlNode_getNextSibling)(IXML_Node *nodeptr);
	const DOMString (*m_ixmlNode_getNodeName)(IXML_Node *nodeptr);
	const DOMString (*m_ixmlNode_getNodeValue)(IXML_Node *nodeptr);
	IXML_NamedNodeMap *(*m_ixmlNode_getAttributes)(IXML_Node *nodeptr);
	void (*m_ixmlDocument_free)(IXML_Document *doc);
	IXML_Node *(*m_ixmlNamedNodeMap_getNamedItem)(
		IXML_NamedNodeMap *nnMap, const DOMString name);
	void (*m_ixmlNamedNodeMap_free)(IXML_NamedNodeMap *nnMap);
	
	// upnp api
	// 1 - Initialization and Registration
	int (*m_UpnpInit)(const char *IPAddress, int Port);
	void (*m_UpnpFinish)();
	unsigned short (*m_UpnpGetServerPort)();
	char *(*m_UpnpGetServerIpAddress)();
	int (*m_UpnpRegisterClient)(Upnp_FunPtr Callback,
		const void *Cookie, UpnpClient_Handle *Hnd);
	int (*m_UpnpUnRegisterClient)(UpnpClient_Handle Hnd);
	// 2 - Discovery
	int (*m_UpnpSearchAsync)(UpnpClient_Handle Hnd, int Mx,
		const char *Target, const void *Cookie);
	// 3 - Control
	int (*m_UpnpGetServiceVarStatus)(UpnpClient_Handle Hnd, const char *ActionURL,
		const char *VarName, DOMString *StVarVal);
	int (*m_UpnpSendAction)(UpnpClient_Handle Hnd, const char *ActionURL,
		const char *ServiceType, const char *DevUDN, IXML_Document *Action,
		IXML_Document **RespNode);
	int (*m_UpnpSendActionAsync)(UpnpClient_Handle Hnd, const char *ActionURL,
		const char *ServiceType, const char *DevUDN, IXML_Document *Action,
		Upnp_FunPtr Callback, const void *Cookie);
	// 4 - Eventing
	int (*m_UpnpSubscribe)(UpnpClient_Handle Hnd,
		const char *PublisherUrl, int *TimeOut, Upnp_SID SubsId);
	int (*m_UpnpUnSubscribe)(UpnpClient_Handle Hnd, Upnp_SID SubsId);
	// 5 - HTTP
	int (*m_UpnpDownloadXmlDoc)(const char *url, IXML_Document **xmlDoc);
	// 6 - Optional Tools API
	int (*m_UpnpResolveURL)(const char *BaseURL,
		const char *RelURL, char *AbsURL);
	IXML_Document *(*m_UpnpMakeAction)(
		const char *ActionName, const char *ServType, int NumArg,
		const char *Arg, ...);
	int (*m_UpnpAddToAction)(
		IXML_Document **ActionDoc, const char *ActionName,
		const char *ServType, const char *ArgName, const char *ArgVal);
	const char *(*m_UpnpGetErrorMessage)(int ErrorCode);
};


class CUPnPControlPoint;

/*
 * Even though we can retrieve the upnpLib handler from the upnpControlPoint,
 * we must pass it separetly at this point, because the class CUPnPControlPoint
 * must be declared after.
 *
 * CUPnPLib can only be removed from the constructor once we agree to link to
 * UPnPLib explicitly, making this dlopen() stuff unnecessary.
 */
template <typename T, char const *XML_ELEMENT_NAME, char const *XML_LIST_NAME>
class CXML_List : public std::map<const wxString, T *>
{
public:
	CXML_List(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *parent,
		const wxString &url);
	~CXML_List();
};


template <typename T, char const *XML_ELEMENT_NAME, char const *XML_LIST_NAME>
CXML_List<T, XML_ELEMENT_NAME, XML_LIST_NAME>::CXML_List(
	const CUPnPControlPoint &upnpControlPoint,
	CUPnPLib &upnpLib,
	IXML_Element *parent,
	const wxString &url)
{
	IXML_Element *elementList =
		upnpLib.Element_GetFirstChildByTag(parent, XML_LIST_NAME);
	unsigned int i = 0;
	for (	IXML_Element *element = upnpLib.Element_GetFirstChildByTag(elementList, XML_ELEMENT_NAME);
		element;
		element = upnpLib.Element_GetNextSiblingByTag(element, XML_ELEMENT_NAME)) {
		// Add a new element to the element list
		T *upnpElement = new T(upnpControlPoint, upnpLib, element, url);
		(*this)[upnpElement->GetKey()] = upnpElement;
		++i;
	}
	wxString msg;
	msg << wxT("\n    ") << char2unicode(XML_LIST_NAME) << wxT(": ") <<
		i << wxT(" ") << char2unicode(XML_ELEMENT_NAME) << wxT("s.");
	AddDebugLogLineM(false, logUPnP, msg);
}


template <typename T, char const *XML_ELEMENT_NAME, char const *XML_LIST_NAME>
CXML_List<T, XML_ELEMENT_NAME, XML_LIST_NAME>::~CXML_List()
{
	typename CXML_List<T, XML_ELEMENT_NAME, XML_LIST_NAME>::iterator it;
	for(it = this->begin(); it != this->end(); ++it) {
		delete (*it).second;
	}
}

#ifdef UPNP_C
	char s_argument[] = "argument";
	char s_argumentList[] = "argumentList";
	char s_action[] = "action";
	char s_actionList[] = "actionList";
	char s_allowedValue[] = "allowedValue";
	char s_allowedValueList[] = "allowedValueList";
	char s_stateVariable[] = "stateVariable";
	char s_serviceStateTable[] = "serviceStateTable";
	char s_service[] = "service";
	char s_serviceList[] = "serviceList";
	char s_device[] = "device";
	char s_deviceList[] = "deviceList";
#else // UPNP_C
	extern char s_argument[];
	extern char s_argumentList[];
	extern char s_action[];
	extern char s_actionList[];
	extern char s_allowedValue[];
	extern char s_allowedValueList[];
	extern char s_stateVariable[];
	extern char s_serviceStateTable[];
	extern char s_service[];
	extern char s_serviceList[];
	extern char s_device[];
	extern char s_deviceList[];
#endif // UPNP_C


class CUPnPArgument;
typedef CXML_List<CUPnPArgument, s_argument, s_argumentList> ArgumentList;
class CUPnPAction;
typedef CXML_List<CUPnPAction, s_action, s_actionList> ActionList;
class CUPnPStateVariable;
typedef CXML_List<CUPnPStateVariable, s_stateVariable, s_serviceStateTable> ServiceStateTable;
class CUPnPAllowedValue;
typedef CXML_List<CUPnPAllowedValue, s_allowedValue, s_allowedValueList> AllowedValueList;
class CUPnPService;
typedef CXML_List<CUPnPService, s_service, s_serviceList> ServiceList;
class CUPnPDevice;
typedef CXML_List<CUPnPDevice, s_device, s_deviceList> DeviceList;


class CUPnPError
{
private:
	IXML_Element *m_root;
	const wxString m_ErrorCode;
	const wxString m_ErrorDescription;
public:
	CUPnPError(
		const CUPnPLib &upnpLib,
		IXML_Document *errorDoc);
	~CUPnPError() {}
	const wxString &getErrorCode() const		{ return m_ErrorCode; }
	const wxString &getErrorDescription() const	{ return m_ErrorDescription; }
};


class CUPnPArgument
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	const wxString m_name;
	const wxString m_direction;
	bool m_retval;
	const wxString m_relatedStateVariable;
	
public:
	CUPnPArgument(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *argument,
		const wxString &SCPDURL);
	~CUPnPArgument() {}
	const wxString &GetName() const		{ return m_name; }
	const wxString &GetDirection() const	{ return m_direction; }
	bool GetRetVal() const			{ return m_retval; }
	const wxString &GetRelatedStateVariable() const	{ return m_relatedStateVariable; }
	const wxString &GetKey() const		{ return m_name; }
};



class CUPnPAction
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	ArgumentList m_ArgumentList;
	const wxString m_name;
	
public:
	CUPnPAction(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *action,
		const wxString &SCPDURL);
	~CUPnPAction() {}
	const wxString &GetName() const		{ return m_name; }
	const wxString &GetKey() const		{ return m_name; }
	const ArgumentList &GetArgumentList() const { return m_ArgumentList; }
};


class CUPnPAllowedValue
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	const wxString m_allowedValue;
	
public:
	CUPnPAllowedValue(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *allowedValue,
		const wxString &SCPDURL);
	~CUPnPAllowedValue() {}
	const wxString &GetAllowedValue() const	{ return m_allowedValue; }
	const wxString &GetKey() const		{ return m_allowedValue; }
};


class CUPnPStateVariable
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	AllowedValueList m_AllowedValueList;
	const wxString m_name;
	const wxString m_dataType;
	const wxString m_defaultValue;
	const wxString m_sendEvents;
	
public:
	CUPnPStateVariable(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *stateVariable,
		const wxString &URLBase);
	~CUPnPStateVariable() {}
	const wxString &GetNname() const	{ return m_name; }
	const wxString &GetDataType() const	{ return m_dataType; }
	const wxString &GetDefaultValue() const	{ return m_defaultValue; }
	const wxString &GetKey() const		{ return m_name; }
	const AllowedValueList &GetAllowedValueList() const { return m_AllowedValueList; }
};


class CUPnPSCPD
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	ActionList m_ActionList;
	ServiceStateTable m_ServiceStateTable;
	const wxString m_SCPDURL;
	
public:
	CUPnPSCPD(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *scpd,
		const wxString &SCPDURL);
	~CUPnPSCPD() {}
	const ActionList &GetActionList() const	{ return m_ActionList; }
	const ServiceStateTable &GetServiceStateTable() const	{ return m_ServiceStateTable; }
};


class CUPnPArgumentValue
{
private:
	wxString m_argument;
	wxString m_value;
	
public:
	CUPnPArgumentValue();
	CUPnPArgumentValue(const wxString &argument, const wxString &value);
	~CUPnPArgumentValue() {}

	const wxString &GetArgument() const	{ return m_argument; }
	const wxString &GetValue() const	{ return m_value; }
	const wxString &SetArgument(const wxString& argument)	{ return m_argument = argument; }
	const wxString &SetValue(const wxString &value)		{ return m_value = value; }
};


class CUPnPService
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	CUPnPLib &m_upnpLib;
	const wxString m_serviceType;
	const wxString m_serviceId;
	const wxString m_SCPDURL;
	const wxString m_controlURL;
	const wxString m_eventSubURL;
	wxString m_absSCPDURL;
	wxString m_absControlURL;
	wxString m_absEventSubURL;
	int m_timeout;
	Upnp_SID m_SID;
	CUPnPSCPD *m_SCPD;
	
public:
	CUPnPService(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *service,
		const wxString &URLBase);
	~CUPnPService();
	
	const wxString &GetServiceType() const	{ return m_serviceType; }
	const wxString &GetServiceId() const	{ return m_serviceId; }
	const wxString &GetSCPDURL() const	{ return m_SCPDURL; }
	const wxString &GetAbsSCPDURL() const	{ return m_absSCPDURL; }
	const wxString &GetControlURL() const	{ return m_controlURL; }
	const wxString &GetEventSubURL() const	{ return m_eventSubURL; }
	const wxString &GetAbsControlURL() const{ return m_absControlURL; }
	const wxString &GetAbsEventSubURL()const{ return m_absEventSubURL; }
	int GetTimeout() const			{ return m_timeout; }
	int *GetTimeoutAddr()			{ return &m_timeout; }
	char *GetSID()				{ return m_SID; }
	const wxString &GetKey() const		{ return m_serviceId; }
	bool IsSubscribed() const		{ return m_SCPD != NULL; }
	void SetSCPD(CUPnPSCPD *SCPD)		{ m_SCPD = SCPD; }
	bool Execute(
		const wxString &ActionName,
		const std::vector<CUPnPArgumentValue> &ArgValue) const;
	const wxString GetStateVariable(const wxString &stateVariableName) const;
};


class CUPnPDevice
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;

	// Please, lock these lists before use
	DeviceList m_DeviceList;
	ServiceList m_ServiceList;
	
	const wxString m_deviceType;
	const wxString m_friendlyName;
	const wxString m_manufacturer;
	const wxString m_manufacturerURL;
	const wxString m_modelDescription;
	const wxString m_modelName;
	const wxString m_modelNumber;
	const wxString m_modelURL;
	const wxString m_serialNumber;
	const wxString m_UDN;
	const wxString m_UPC;
	wxString m_presentationURL;
	
public:
	CUPnPDevice(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *device,
		const wxString &URLBase);
	~CUPnPDevice() {}
	
	const wxString &GetUDN() const			{ return m_UDN; }
	const wxString &GetDeviceType() const		{ return m_deviceType; }
	const wxString &GetFriendlyName() const		{ return m_friendlyName; }
	const wxString &GetPresentationURL() const	{ return m_presentationURL; }
	const wxString &GetKey() const			{ return m_UDN; }
};


class CUPnPRootDevice : public CUPnPDevice
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	const wxString m_URLBase;
	const wxString m_location;
	int m_expires;

public:
	CUPnPRootDevice(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *rootDevice,
		const wxString &OriginalURLBase,
		const wxString &FixedURLBase,
		const char *location,
		int expires);
	~CUPnPRootDevice() {}
	
	const wxString &GetURLBase() const		{ return m_URLBase; }
	const wxString &GetLocation() const		{ return m_location; }
	int GetExpires() const				{ return m_expires; }

	void SetExpires(int expires)			{ m_expires = expires; }
};


typedef std::map<const wxString, CUPnPRootDevice *> RootDeviceList;


class CUPnPControlPoint
{
private:
	static CUPnPControlPoint *s_CtrlPoint;
	// upnp stuff
	CUPnPLib m_upnpLib;
	UpnpClient_Handle m_UPnPClientHandle;
	RootDeviceList m_RootDeviceList;
	wxMutex m_RootDeviceListMutex;
	bool m_IGWDeviceDetected;
	bool m_WanServiceDetected;
	CUPnPService *m_WanService;
	wxMutex m_WaitForSearchTimeout;

public:
	CUPnPControlPoint(unsigned short udpPort);
	~CUPnPControlPoint();
	bool AcquirePortList(const amuleIPV4Address portArray[], int n);
	void Subscribe(CUPnPService &service);
	void Unsubscribe(CUPnPService &service);
	
	UpnpClient_Handle GetUPnPClientHandle()	const
		{ return m_UPnPClientHandle; }

	bool GetIGWDeviceDetected() const
		{ return m_IGWDeviceDetected; }
	void SetIGWDeviceDetected(bool b)
		{ m_IGWDeviceDetected = b; }
	bool GetWanServiceDetected() const
		{ return m_WanServiceDetected; }
	void SetWanServiceDetected(bool b)
		{ m_WanServiceDetected = b; }
	void SetWanService(CUPnPService *service)
		{ m_WanService = service; }

	// Callback function
	static int Callback(Upnp_EventType EventType, void* Event, void* Cookie);
	
private:
	void OnEventReceived(
		const wxString &Sid,
		int EventKey,
		IXML_Document *ChangedVariables);
	void AddRootDevice(
		IXML_Element *rootDevice,
		const wxString &urlBase,
		const char *location,
		int expires);
	void RemoveRootDevice(const char *udn);
};


#endif /* AMULE_UPNP_H */

// File_checked_for_headers
