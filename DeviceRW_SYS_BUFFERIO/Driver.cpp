/************************************************************************
* 文件名称:Driver.cpp                                                 
* 作    者:张帆
* 完成日期:2007-11-1
*************************************************************************/

#include "Driver.h"

/*Add by CZHC 2013/8/3 13:12*/
void TestLookaside()
{
	KdPrint(("enter TestLookaside ......\n"));
	PAGED_LOOKASIDE_LIST tLookaside;
	ExInitializePagedLookasideList(&tLookaside,NULL,NULL,0,sizeof(MyListNode),'1234',0);

	MyListNode* MyList[10] = {0};
	for(int i=0; i<10; i++)
	{
		MyList[i] = (MyListNode*)ExAllocateFromPagedLookasideList(&tLookaside);
		MyList[i]->data = i;
	}

	for(int k=0; k<10; k++)
	{
		KdPrint(("%d   ",MyList[k]->data));
		ExFreeToPagedLookasideList(&tLookaside,MyList[k]);
		MyList[k] = NULL;
	}

	ExDeletePagedLookasideList(&tLookaside);
	KdPrint(("leave TestLookaside ......\n"));

}

/*Add by CZHC 2013/8/1 21:54*/
void TestList()
{
	LIST_ENTRY tHead;
	InitializeListHead(&tHead);

	MyListNode* pData;
	ULONG i=0;

	KdPrint(("begin insert data to list \n"));
	for(i=0; i<10; i++)
	{
		KdPrint(("insert one %d\n",i));
		pData = (MyListNode*)ExAllocatePool(PagedPool,sizeof(MyListNode));
		pData->data = i;
		InsertHeadList(&tHead,&pData->ListEntry);
	}

	KdPrint(("begin remove from link and print"));
	while(!IsListEmpty(&tHead))
	{
		MyListNode* pCurNode = (MyListNode*)RemoveHeadList(&tHead);
		KdPrint(("%d  ",pCurNode->data));
		ExFreePool(pCurNode);
	}
}

/*Add by CZHC 2013/8/1 20:20*/
void PrintInfo(IN PDRIVER_OBJECT pDriverObject)
{
	KdPrint(("------------------------------\n"));
	KdPrint(("Enter PrintInfo\n"));
	NTSTATUS tStatus;
	PDEVICE_OBJECT pDevice = pDriverObject->DeviceObject;
	int i = 0;
	for(; pDevice != NULL; pDevice = pDevice->NextDevice)
	{
		KdPrint(("The %d Device\n",i++));
		KdPrint(("Device AttachDevice:0x%08X\n",pDevice->AttachedDevice));
		KdPrint(("Next Device:0x%08X\n",pDevice->NextDevice));
		KdPrint(("Device stacksize:%d\n",pDevice->StackSize));
		KdPrint(("Device's DriverObject 0X%08X\n",pDevice->DriverObject));
	}
	KdPrint(("Leaver PrintInfo\n"));
	KdPrint(("------------------------------\n"));

	KdPrint(("------------------------------\n"));
	PEPROCESS pEProcess = PsGetCurrentProcess();
	PTSTR ProcessName = (PTSTR)((ULONG)pEProcess + 0x174);
	KdPrint(("%s\n",ProcessName));
	KdPrint(("------------------------------\n"));
}

/************************************************************************
* 函数名称:DriverEntry
* 功能描述:初始化驱动程序，定位和申请硬件资源，创建内核对象
* 参数列表:
      pDriverObject:从I/O管理器中传进来的驱动对象
      pRegistryPath:驱动程序在注册表的中的路径
* 返回 值:返回初始化驱动状态
*************************************************************************/

#pragma INITCODE
extern "C" NTSTATUS DriverEntry (
			IN PDRIVER_OBJECT pDriverObject,
			IN PUNICODE_STRING pRegistryPath	) 
{
	NTSTATUS status;
	KdPrint(("Enter DriverEntry\n"));

	//注册其他驱动调用函数入口
	pDriverObject->DriverUnload = HelloDDKUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = HelloDDKDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = HelloDDKDispatchRoutine;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = HelloDDKDeviceWrite;
	pDriverObject->MajorFunction[IRP_MJ_READ] = HelloDDKDeviceRead;
	
	//创建驱动设备对象
	status = CreateDevice(pDriverObject);

	PrintInfo(pDriverObject);
	KdPrint(("DriverEntry end\n"));

	//TestList();
	//TestLookaside();
	return status;
}

/************************************************************************
* 函数名称:CreateDevice
* 功能描述:初始化设备对象
* 参数列表:
      pDriverObject:从I/O管理器中传进来的驱动对象
* 返回 值:返回初始化状态
*************************************************************************/
#pragma INITCODE
NTSTATUS CreateDevice (IN PDRIVER_OBJECT pDriverObject) 
{
	NTSTATUS status;
	PDEVICE_OBJECT pDevObj;
	PDEVICE_EXTENSION pDevExt;
	
	//创建设备名称
	UNICODE_STRING devName;
	RtlInitUnicodeString(&devName,L"\\Device\\MyHelloDDKDeviceRW");
	
	//创建设备
	status = IoCreateDevice( pDriverObject,
						sizeof(DEVICE_EXTENSION),
						&(UNICODE_STRING)devName,
						FILE_DEVICE_UNKNOWN,
						0, TRUE,
						&pDevObj );
	if (!NT_SUCCESS(status))
		return status;

	pDevObj->Flags |= DO_BUFFERED_IO;
	pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	pDevExt->pDevice = pDevObj;
	pDevExt->ustrDeviceName = devName;
	pDevExt->buflen = 128;
	pDevExt->dataBuf = (PVOID)ExAllocatePool(PagedPool,pDevExt->buflen);

	//创建符号链接
	UNICODE_STRING symLinkName;
	RtlInitUnicodeString(&symLinkName,L"\\??\\HelloDDKDeviceRW");
	pDevExt->ustrSymLinkName = symLinkName;
	status = IoCreateSymbolicLink( &symLinkName,&devName );
	if (!NT_SUCCESS(status)) 
	{
		IoDeleteDevice( pDevObj );
		return status;
	}
	return STATUS_SUCCESS;
}

/************************************************************************
* 函数名称:HelloDDKUnload
* 功能描述:负责驱动程序的卸载操作
* 参数列表:
      pDriverObject:驱动对象
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
VOID HelloDDKUnload (IN PDRIVER_OBJECT pDriverObject) 
{
	PDEVICE_OBJECT	pNextObj;
	KdPrint(("Enter DriverUnload\n"));
	pNextObj = pDriverObject->DeviceObject;
	while (pNextObj != NULL) 
	{
		PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)
			pNextObj->DeviceExtension;

		ExFreePool(pDevExt->dataBuf);
		pDevExt->buflen = 0;

		//删除符号链接
		UNICODE_STRING pLinkName = pDevExt->ustrSymLinkName;
		IoDeleteSymbolicLink(&pLinkName);
		pNextObj = pNextObj->NextDevice;
		IoDeleteDevice( pDevExt->pDevice );
	}
}

/************************************************************************
* 函数名称:HelloDDKDispatchRoutine
* 功能描述:对读IRP进行处理
* 参数列表:
      pDevObj:功能设备对象
      pIrp:从IO请求包
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS HelloDDKDispatchRoutine(IN PDEVICE_OBJECT pDevObj,
								 IN PIRP pIrp) 
{
	KdPrint(("Enter HelloDDKDispatchRoutine\n"));
	NTSTATUS status = STATUS_SUCCESS;
	// 完成IRP
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;	// bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	KdPrint(("Leave HelloDDKDispatchRoutine\n"));
	pIrp->MdlAddress->
	return status;
}

/************************************************************************
* 函数名称:HelloDDKDeviceReadWrite
* 功能描述:对读IRP进行处理
* 参数列表:
      pDevObj:功能设备对象
      pIrp:从IO请求包
* 返回 值:返回状态
*************************************************************************/
#pragma PAGEDCODE
NTSTATUS HelloDDKDeviceRead(IN PDEVICE_OBJECT pDevObj,IN PIRP pIrp) 
{
	KdPrint(("Enter HelloDDKDeviceRead\n"));
	NTSTATUS status = STATUS_SUCCESS;
	
	//得到当前IO堆栈
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
	//得到要读取的字节数
	ULONG tWantToReadLen = stack->Parameters.Read.Length;
	
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	if(tWantToReadLen > pDevExt->buflen)
	{
		pIrp->IoStatus.Status = STATUS_FILE_INVALID;
		IoCompleteRequest(pIrp,IO_NO_INCREMENT);//IO_NO_INCREMENT   线程被唤醒优先级
		KdPrint(("u want to read to much \n"));
		KdPrint(("Leave HelloDDKDeviceRead\n"));
		return STATUS_UNSUCCESSFUL;
	}
	
	// 完成IRP
	memcpy(pIrp->AssociatedIrp.SystemBuffer,pDevExt->dataBuf,tWantToReadLen);
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = tWantToReadLen;	// bytes xfered
	IoCompleteRequest( pIrp, IO_NO_INCREMENT);
	KdPrint(("Leave HelloDDKDeviceRead\n"));

	return status;
}

#pragma PAGEDCODE
NTSTATUS HelloDDKDeviceWrite(IN PDEVICE_OBJECT pDevObj,IN PIRP pIrp) 
{
	KdPrint(("Enter HelloDDKDeviceWrite\n"));
	NTSTATUS status = STATUS_SUCCESS;
	
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
	if(stack->Parameters.Write.ByteOffset.HighPart != 0)
	{
		pIrp->IoStatus.Status = STATUS_FILE_INVALID;
		IoCompleteRequest(pIrp,IO_NO_INCREMENT);//IO_NO_INCREMENT   线程被唤醒优先级
		KdPrint(("Offsert Highpart not equal 0\n"));
		KdPrint(("Leave HelloDDKDeviceWrite\n"));
		return STATUS_UNSUCCESSFUL;
	}
	ULONG	 tWantToWriteLen = stack->Parameters.Write.Length;
	ULONG	 tWriteOffset	 = stack->Parameters.Write.ByteOffset.LowPart;
	
	//
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;

	if(tWantToWriteLen>pDevExt->buflen)
	{
		pIrp->IoStatus.Status = STATUS_FILE_INVALID;
		IoCompleteRequest(pIrp,IO_NO_INCREMENT);//IO_NO_INCREMENT   线程被唤醒优先级
		KdPrint(("u want to write too much\n"));
		KdPrint(("Levae HelloDDKDeviceWrite\n"));
		return STATUS_UNSUCCESSFUL;
	}
	memcpy(pDevExt->dataBuf,pIrp->AssociatedIrp.SystemBuffer,tWantToWriteLen);
	pIrp->IoStatus.Information = tWantToWriteLen;
	pIrp->IoStatus.Status	   = STATUS_SUCCESS;
	
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
	KdPrint(("Leave HelloDDKDeviceWrite\n"));

	return status;
}