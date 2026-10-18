#include "utils.h"
#include "callback.h"
#include "timer.h"
#include "ssdt.h"
#include "idt.h"
#include "thread.h"
#include "filter.h"
#include "worker.h"
#include "driver.h"

NTSTATUS WfpFilterQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
NTSTATUS WfpCalloutQuery(PVOID outputBuffer, ULONG outputBufferLength, ULONG_PTR* bytesWritten);
#include "object.h"

PDRIVER_OBJECT g_DriverObject = NULL;
#define DEVICE_OBJECT_NAME  L"\\Device\\liunong"
#define DEVICE_LINK_NAME    L"\\DosDevices\\liunonglink"

void DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DefaultDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS IoControlHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status;

	g_DriverObject = DriverObject;

	CallbackInit();
	TimerInit();
	SsdtInit();
	WorkerInit();

	UNICODE_STRING deviceName = { 0 };
	UNICODE_STRING symbolicLinkName = { 0 };
	DEVICE_OBJECT* deviceObject = nullptr;

	RtlInitUnicodeString(&deviceName, DEVICE_OBJECT_NAME);
	RtlInitUnicodeString(&symbolicLinkName, DEVICE_LINK_NAME);
	status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);
	if (!NT_SUCCESS(status)) {
		DbgPrint("[MyArkDrv] Failed to create device: 0x%X\n", status);
		return status;
	}

	DriverObject->DeviceObject = deviceObject;

	status = IoCreateSymbolicLink(&symbolicLinkName, &deviceName);
	if (!NT_SUCCESS(status)) {
		DbgPrint("[MyArkDrv] Failed to create symbolic link: 0x%X", status);
		IoDeleteDevice(deviceObject);
		return status;
	}

	DriverObject->DriverUnload = DriverUnload;
	DbgPrint("[MyArkDrv] Driver loaded.\n");

	for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
		DriverObject->MajorFunction[i] = DefaultDispatch;
	}

	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoControlHandler;
	return STATUS_SUCCESS;
}


NTSTATUS DefaultDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING symbolicLinkName = { 0 };
	RtlInitUnicodeString(&symbolicLinkName, DEVICE_LINK_NAME);
	IoDeleteSymbolicLink(&symbolicLinkName);

	if (DriverObject->DeviceObject)
		IoDeleteDevice(DriverObject->DeviceObject);

	DbgPrint("[MyArkDrv] Driver unloaded.\n");
}

NTSTATUS IoControlHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

	ULONG ioControlCode = stack->Parameters.DeviceIoControl.IoControlCode;
	PVOID outputBuffer = Irp->AssociatedIrp.SystemBuffer;
	ULONG outputBufferLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
	PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
	ULONG inputBufferLength = stack->Parameters.DeviceIoControl.InputBufferLength;

	NTSTATUS status;
	ULONG_PTR bytesWritten = 0;

	switch (ioControlCode)
	{
	case IOCTL_MYARK_QUERY_REG_CALLBACKS:
		status = CallbackQueryReg(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_OB_CALLBACKS:
		status = CallbackQueryOb(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_PROCERSS_CALLBACKS:
		status = CallbackQueryProcess(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_BUGCHECK_CALLBACKS:
		status = CallbackQueryBugCheckReason(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_SHUTDOWN_CALLBACKS:
		status = CallbackQueryLastChanceShutdown(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_BUGCHECK_REGISTERS:
		status = CallbackQueryBugCheck(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_NOTIFY_SHUTDOWN:
		status = CallbackQueryNotifyShutdown(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_TIMERS:
		status = TimerQuery(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_SSDT:
		status = SsdtQuery(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_SHADOW_SSDT:
		status = SsdtQueryShadow(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_IDT:
		status = IdtQuery(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_SYSTEM_THREADS:
		status = SystemThreadQuery(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_FILTERS:
		status = FilterQuery(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_KERNEL_OBJECT:
		status = KernelObjectQuery(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_WORK_THREADS:
		status = WorkerQuery(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_WFP_FILTERS:
		status = WfpFilterQuery(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_WFP_CALLOUTS:
		status = WfpCalloutQuery(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_UNLOADED_DRIVERS:
	{
		ULONG source = MYARK_UNLOADED_SOURCE_MM;
		if (inputBuffer && inputBufferLength >= sizeof(MYARK_UNLOADED_DRIVER_REQUEST))
			source = ((MYARK_UNLOADED_DRIVER_REQUEST*)inputBuffer)->Source;
		if (source == MYARK_UNLOADED_SOURCE_PIDDB)
			status = UnloadDriverQuery2(outputBuffer, outputBufferLength, &bytesWritten);
		else if (source == MYARK_UNLOADED_SOURCE_CI)
			status = UnloadDriverQuery3(outputBuffer, outputBufferLength, &bytesWritten);
		else
			status = UnloadDriverQuery1(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	}
	case IOCTL_MYARK_QUERY_DRIVER:
		status = DriverQuery(outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_DRIVER_DEVICES:
		status = DriverDeviceQuery(inputBuffer, inputBufferLength, outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_DRIVER_DISPATCH:
		status = DriverDispatchQuery(inputBuffer, inputBufferLength, outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_QUERY_OBJECT_TYPE_PROCEDURES:
		status = ObjectTypeProcedureQuery(inputBuffer, inputBufferLength, outputBuffer, outputBufferLength, &bytesWritten);
		break;
	case IOCTL_MYARK_READ_MEMORY:
		status = ReadMemory(inputBuffer, inputBufferLength, outputBuffer, outputBufferLength, &bytesWritten);
		break;
	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = bytesWritten;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
