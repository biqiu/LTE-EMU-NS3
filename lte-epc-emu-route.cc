#include "ns3/abort.h"
#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"
#include "ns3/emu-module.h"
#include "ns3/emu-helper.h"
#include "ns3/internet-module.h"

#include <string>
#include <fstream>
#include <vector>
#include <libxml/encoding.h>
#include <libxml/xmlreader.h>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("LteEpcEmu");

class LteEpcEmu
{
public:
	LteEpcEmu();
	~LteEpcEmu();
	bool Init();
	bool Create();
	int Run();
private:	
	void ReadSenderConfig(xmlNodePtr pRoot);
	void ReadReceiverConfig(xmlNodePtr pRoot);
	void ReadUeConfig(xmlNodePtr pRoot);
	void ReadSimulatorConfig(xmlNodePtr pRoot);
	bool CreatePgw();
	bool CreateLte();
	bool SetUeMobility();
	bool InstallDevices();
	bool AssignIpForUe();
	bool AddEmu(Ptr<Node> node,string EmuIp, string Mask);
	bool AddRoute();
	bool AttachUeEnb();
private:
	uint16_t numberOfNodes;
	double distance;
	double velocity;
	double stopTime;
	string senderGateway;
	string receiverGateway;
	string senderMask;
	string receiverMask;
	string receiverIp;
	Ptr<LteHelper> lteHelper;
	Ptr<EpcHelper> epcHelper;
	Ptr<Node> pgw;	
	NetDeviceContainer internetDevices;
	Ipv4StaticRoutingHelper ipv4RoutingHelper;
	NodeContainer ueNodes;
  	NodeContainer enbNodes;	
	NetDeviceContainer ueLteDevs;
	NetDeviceContainer enbLteDevs;
};

LteEpcEmu::LteEpcEmu()
{
	NS_LOG_FUNCTION(this);
	GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  	GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
	Config::SetDefault ("ns3::LteAmc::AmcModel", EnumValue (LteAmc::PiroEW2010));
	numberOfNodes = 1;
	distance = 0.0;
	velocity = 0.0;
	stopTime = 100.0;
	receiverIp = "202.117.10.89";
	senderGateway = "202.117.15.1";
	receiverGateway = "202.117.10.1";
	senderMask = "255.255.255.0";
	receiverMask = "255.255.255.0";
	
}

LteEpcEmu::~LteEpcEmu()
{	
	NS_LOG_FUNCTION(this);
}

bool LteEpcEmu::Init()
{
	NS_LOG_FUNCTION(this);
	//从配置文件中读入发送端网关、接收端网关、UE和Enb之间的初始距离、Ue的速度、接收端IP sennderGateway receiverGateway distance velocity receiverIp
	const string strFile("scratch/subdir/Config/Config.xml");
	xmlDocPtr pdoc = NULL;
	xmlNodePtr pRoot = NULL;
	pdoc = xmlReadFile(strFile.c_str(),"UTF-8",XML_PARSE_RECOVER);
	if(NULL == pdoc)
		exit(1);
	pRoot = xmlDocGetRootElement(pdoc);
	if(NULL == pRoot)
		exit(2);
	if(xmlStrcmp(pRoot->name,BAD_CAST "LteSimulatorConfig"))
	{
		xmlFreeDoc(pdoc);
		exit(3);
	}
	//读配置
	ReadSenderConfig(pRoot);
	ReadReceiverConfig(pRoot);
	ReadUeConfig(pRoot);
	ReadSimulatorConfig(pRoot);

	xmlFreeDoc(pdoc);
	xmlCleanupParser();
	return true;
}

bool LteEpcEmu::Create()
{
	NS_LOG_FUNCTION(this);
	CreatePgw();
	CreateLte();
	return true;
}

void LteEpcEmu::ReadSenderConfig(xmlNodePtr pRoot)
{
	xmlChar* szAttr = NULL;
	xmlNodePtr pcurnode = pRoot->xmlChildrenNode;
	while(NULL != pcurnode)
	{
		if((xmlStrcmp(pcurnode->name, BAD_CAST "SenderConfig") ==0))
		{		
			if(xmlHasProp(pcurnode, BAD_CAST "senderGateway"))
			{
				szAttr = xmlGetProp(pcurnode,BAD_CAST"senderGateway");
				senderGateway = (char*)szAttr;
			}
		
			if(xmlHasProp(pcurnode, BAD_CAST "senderMask"))
			{
				szAttr = xmlGetProp(pcurnode,BAD_CAST "senderMask");
				senderMask = (char*)szAttr;
			}
			break;
		}
		pcurnode = pcurnode->next;
	}
	return;
}

void LteEpcEmu::ReadReceiverConfig(xmlNodePtr pRoot)
{	
	xmlChar* szAttr = NULL;
	xmlNodePtr pcurnode = pRoot->xmlChildrenNode;
	while(NULL != pcurnode)
	{
		if((xmlStrcmp(pcurnode->name, BAD_CAST "ReceiverConfig") ==0))
		{		
			if(xmlHasProp(pcurnode, BAD_CAST "receiverIp"))
			{
				szAttr = xmlGetProp(pcurnode,BAD_CAST "receiverIp");
				receiverIp = (char*)szAttr;
			}
			if(xmlHasProp(pcurnode, BAD_CAST "receiverGateway"))
			{
				szAttr = xmlGetProp(pcurnode,BAD_CAST "receiverGateway");
				receiverGateway = (char*)szAttr;
			}
			if(xmlHasProp(pcurnode, BAD_CAST "receiverMask"))
			{
				szAttr = xmlGetProp(pcurnode,BAD_CAST "receiverMask");
				receiverMask = (char*)szAttr;
			}
			break;
		}
		pcurnode = pcurnode->next;
	}
	return;
}

void LteEpcEmu::ReadUeConfig(xmlNodePtr pRoot)
{
	xmlChar* szAttr = NULL;
	xmlNodePtr pcurnode = pRoot->xmlChildrenNode;
	while(NULL != pcurnode)
	{
		if(xmlStrcmp(pcurnode->name,BAD_CAST "UeConfig") == 0)
		{			
			if(xmlHasProp(pcurnode,BAD_CAST "distance"))
			{
				szAttr = xmlGetProp(pcurnode,BAD_CAST "distance");
				distance = atof((const char*)szAttr);
			}
			if(xmlHasProp(pcurnode,BAD_CAST "velocity"))
			{
				szAttr = xmlGetProp(pcurnode,BAD_CAST "velocity");
				velocity = atof((const char*)szAttr);
			}
		}
		pcurnode = pcurnode->next;
	}
	return;
}

void LteEpcEmu::ReadSimulatorConfig(xmlNodePtr pRoot)
{
	xmlChar* szAttr = NULL;
	xmlNodePtr pcurnode = pRoot->xmlChildrenNode;
	while(NULL != pcurnode)
	{
		if(xmlStrcmp(pcurnode->name,BAD_CAST "SimulatorStopTime") == 0)
		{
			if(xmlHasProp(pcurnode,BAD_CAST "stopTime"))
			{
				szAttr = xmlGetProp(pcurnode,BAD_CAST "stopTime");
				stopTime = atof((const char*)szAttr);
			}
			break;			
		}
		pcurnode = pcurnode->next;
	}
	return;
}
	
bool LteEpcEmu::CreatePgw()
{
	lteHelper = CreateObject<LteHelper> ();
  	epcHelper = CreateObject<EpcHelper> ();
  	lteHelper->SetEpcHelper (epcHelper);
	pgw = epcHelper->GetPgwNode ();	
	AddEmu(pgw,senderGateway, senderMask);//add EMU for pgw and assign IP Address
	return true;
}

bool LteEpcEmu::CreateLte()
{
	NS_LOG_FUNCTION(this);
	enbNodes.Create(numberOfNodes);
  	ueNodes.Create(numberOfNodes);
	InternetStackHelper internet;
  	internet.Install (ueNodes);
	
	SetUeMobility();	
	InstallDevices();
	AssignIpForUe();	
	AddEmu(ueNodes.Get(0),receiverGateway,receiverMask);//add EMU for UE
	AddRoute();	
	AttachUeEnb();
	return true;
}

bool LteEpcEmu::AddEmu(Ptr<Node> node,string EmuIp, string Mask)
{
	NS_LOG_FUNCTION(this);
  	Ptr<EmuNetDevice> firstEmuDev = CreateObject<EmuNetDevice> ();
  	firstEmuDev->SetAttribute ("Address", Mac48AddressValue (Mac48Address::Allocate ()));//设置设备属性
  	firstEmuDev->SetAttribute ("DeviceName", StringValue ("eth0"));
    firstEmuDev->SetAttribute ("RxQueueSize", UintegerValue (100000));

  	Ptr<Queue> firstQueue = CreateObject<DropTailQueue> ();
  	firstEmuDev->SetQueue (firstQueue);

  	node->AddDevice (firstEmuDev);
	
  	Ptr<Ipv4> firstIpv4 = node->GetObject<Ipv4> ();
  	uint32_t firstInterface = firstIpv4->AddInterface (firstEmuDev);
  	Ipv4InterfaceAddress address1 = Ipv4InterfaceAddress (EmuIp.c_str(), Mask.c_str());
  	firstIpv4->AddAddress (firstInterface, address1);
  	firstIpv4->SetMetric (firstInterface, 1);
  	firstIpv4->SetUp (firstInterface);
	return true;
}

bool LteEpcEmu::SetUeMobility()
{
	NS_LOG_FUNCTION(this);
	MobilityHelper mobility;
	//1.Mobility Model for enb	
    	Ptr<ListPositionAllocator> positionAllocForEnb = CreateObject<ListPositionAllocator> ();
    	for (uint16_t i = 0; i < numberOfNodes; i++)
      		positionAllocForEnb->Add (Vector(0, 0, 0));     	
   	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
   	mobility.SetPositionAllocator(positionAllocForEnb);
    	mobility.Install(enbNodes);

   	//2.Mobility Model for ue
    	Ptr<ListPositionAllocator> positionAllocForUe = CreateObject<ListPositionAllocator> ();
    	for (uint16_t i = 0; i < numberOfNodes; i++)
     		positionAllocForUe->Add (Vector(distance, 0, 0));
    	const ns3::Vector3D speed(velocity,0.0,0.0);
    	mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel","Velocity",Vector3DValue(speed)); 
   	mobility.SetPositionAllocator(positionAllocForUe);
    	mobility.Install(ueNodes);
	
    	Ptr<ConstantVelocityMobilityModel> VMM_0_0;	
    	VMM_0_0 = ueNodes.Get(0) -> GetObject<ConstantVelocityMobilityModel>();
    	VMM_0_0 -> SetVelocity(Vector3D(velocity,0.0,0.0));
	return true;
}

bool LteEpcEmu::InstallDevices()
{
	NS_LOG_FUNCTION(this);
  	enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);         
  	ueLteDevs = lteHelper->InstallUeDevice (ueNodes);
	return true;
}

bool LteEpcEmu::AssignIpForUe()
{
	NS_LOG_FUNCTION(this);
  	Ipv4InterfaceContainer ueIpIface;
  	ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
	return true;
}

bool LteEpcEmu::AddRoute()
{
	NS_LOG_FUNCTION(this);
	//为UE加入默认路由
	for(uint32_t u = 0; u < ueNodes.GetN(); ++u)
  	{
    		Ptr<Node> ueNode = ueNodes.Get(u); 
    		Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
    		ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(),1);		
  	}	
	//PGW节点添加默认路由
	Ptr<Ipv4StaticRouting> pgwStaticRouting = ipv4RoutingHelper.GetStaticRouting (pgw->GetObject<Ipv4>());
	pgwStaticRouting->AddHostRouteTo(Ipv4Address(receiverIp.c_str()),Ipv4Address("7.0.0.2"),1);
	pgwStaticRouting->AddNetworkRouteTo (Ipv4Address (receiverIp.c_str()), Ipv4Mask (receiverMask.c_str()), 3);	
	return true;
}

bool LteEpcEmu::AttachUeEnb()
{
	NS_LOG_FUNCTION(this);
	//Attach one UE per eNodeB	
	for(uint16_t i = 0; i < numberOfNodes; ++i)
	{
  		lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(0));
	}
	return true;
}

int LteEpcEmu::Run()
{
	NS_LOG_FUNCTION(this);
	//Simulator::Stop(Seconds(stopTime));
  	Simulator::Run();
  	Simulator::Destroy();
	return 0;
}

int main(int argc, char *argv[])
{
	LogComponentEnable("LteEpcEmu",LOG_LEVEL_ALL);
	LteEpcEmu LteSimulator;
	LteSimulator.Init();
	LteSimulator.Create();
	return LteSimulator.Run();
	return 0;
}
