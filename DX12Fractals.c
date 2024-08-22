#include <stdio.h>
#include <stdint.h>

#define COBJMACROS
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#pragma comment(linker, "/DEFAULTLIB:D3d12.lib")
#pragma comment(linker, "/DEFAULTLIB:DXGI.lib")
#pragma comment(linker, "/DEFAULTLIB:dxguid.lib")

//c23 compatibility stuff:
#include <stdbool.h>
#include <stdalign.h>
#define nullptr ((void*)0)

__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

HANDLE ConsoleHandle;

ID3D12Device* Device;

inline void THROW_ON_FAIL_IMPL(HRESULT hr, int line)
{
	if (hr == 0x887A0005)//device removed
	{
		THROW_ON_FAIL_IMPL(ID3D12Device_GetDeviceRemovedReason(Device), line);
	}

	if (FAILED(hr))
	{
		LPWSTR messageBuffer;
		DWORD formattedErrorLength = FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			hr,
			MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
			(LPWSTR)&messageBuffer,
			0,
			nullptr
		);

		if (formattedErrorLength == 0)
			WriteConsoleA(ConsoleHandle, "an error occured, unable to retrieve error message\n", 51, nullptr, nullptr);
		else
		{
			WriteConsoleA(ConsoleHandle, "an error occured: ", 18, nullptr, nullptr);
			WriteConsoleW(ConsoleHandle, messageBuffer, formattedErrorLength, nullptr, nullptr);
			WriteConsoleA(ConsoleHandle, "\n", 1, nullptr, nullptr);
		}

		char buffer[50];
		int stringlength = _snprintf_s(buffer, 50, _TRUNCATE, "error code: 0x%X\nlocation:line %i\n", hr, line);
		WriteConsoleA(ConsoleHandle, buffer, stringlength, nullptr, nullptr);



		RaiseException(0, EXCEPTION_NONCONTINUABLE, 0, nullptr);
	}
}

#define THROW_ON_FAIL(x) THROW_ON_FAIL_IMPL(x, __LINE__)

#define THROW_ON_FALSE(x) if((x) == FALSE) THROW_ON_FAIL(GetLastError())

#define VALIDATE_HANDLE(x) if((x) == nullptr || (x) == INVALID_HANDLE_VALUE) THROW_ON_FAIL(GetLastError())

#define countof(x) (sizeof(x) / sizeof(x[0]))

#define UPLOAD_DATA_SIGNAL_VALUE 1
#define COMPUTE_EXECUTED_SIGNAL_VALUE 2

int main()
{
	ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

#if defined(DEBUG) || defined(_DEBUG)
	ID3D12Debug* DebugInterface;
	THROW_ON_FAIL(D3D12GetDebugInterface(&IID_ID3D12Debug, &DebugInterface));
	ID3D12Debug_EnableDebugLayer(DebugInterface);
#endif // DEBUG

	UINT DxgiFactoryFlags = 0;

#if defined(DEBUG) || defined(_DEBUG)
	DxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif // DEBUG

	IDXGIFactory6* Factory;
	THROW_ON_FAIL(CreateDXGIFactory2(DxgiFactoryFlags, &IID_IDXGIFactory6, (void**)&Factory));
	
	IDXGIAdapter* Adapter;

	const bool useWARPAdapter = false;

	if (useWARPAdapter)
	{
		THROW_ON_FAIL(IDXGIFactory6_EnumWarpAdapter(Factory, &IID_IDXGIAdapter, (void**)&Adapter));
	}
	else
	{
		IDXGIFactory6_EnumAdapterByGpuPreference(Factory, 0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, &IID_IDXGIAdapter1, &Adapter);
	}
		
	THROW_ON_FAIL(D3D12CreateDevice((IUnknown*)Adapter, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device, (void**)&Device));
	
	
	D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { .HighestShaderModel = D3D_HIGHEST_SHADER_MODEL };
	THROW_ON_FAIL(ID3D12Device_CheckFeatureSupport(Device, D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)));

	const int minor = shaderModel.HighestShaderModel & 0x0f;
	const int major = shaderModel.HighestShaderModel >> 4;
	printf("Current device support highest shader model: %i.%i\n", major, minor);

	D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSignature = { 0 };
	rootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	THROW_ON_FAIL(ID3D12Device_CheckFeatureSupport(Device, D3D12_FEATURE_ROOT_SIGNATURE, &rootSignature, sizeof(rootSignature)));
	
	const char* signatureVersion = "1.0";
	switch (rootSignature.HighestVersion)
	{
		case D3D_ROOT_SIGNATURE_VERSION_1_0:
		default:
			break;

		case D3D_ROOT_SIGNATURE_VERSION_1_1:
			signatureVersion = "1.1";
			break;
	}
	printf("Current device supports highest root signature version: %s\n", signatureVersion);

	{
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels = { 0 };
		msQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		msQualityLevels.SampleCount = 4;
		msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
		msQualityLevels.NumQualityLevels = 0;

		THROW_ON_FAIL(ID3D12Device_CheckFeatureSupport(Device, D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));

		printf("msaaQuality: %u\n", msQualityLevels.NumQualityLevels);
	}
	puts("\n================================================\n");


#if defined(DEBUG) || defined(_DEBUG)
	{
		ID3D12InfoQueue* InfoQueue;
		THROW_ON_FAIL(ID3D12Device_QueryInterface(Device, &IID_ID3D12InfoQueue, &InfoQueue));

		THROW_ON_FAIL(ID3D12InfoQueue_SetBreakOnSeverity(InfoQueue, D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
		THROW_ON_FAIL(ID3D12InfoQueue_SetBreakOnSeverity(InfoQueue, D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
		THROW_ON_FAIL(ID3D12InfoQueue_SetBreakOnSeverity(InfoQueue, D3D12_MESSAGE_SEVERITY_WARNING, TRUE));

		D3D12_MESSAGE_ID MessageIDs[] = {
			D3D12_MESSAGE_ID_DEVICE_CLEARVIEW_EMPTYRECT
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = { 0 };
		NewFilter.DenyList.NumSeverities = 0;
		NewFilter.DenyList.pSeverityList = nullptr;
		NewFilter.DenyList.NumIDs = countof(MessageIDs);
		NewFilter.DenyList.pIDList = &MessageIDs;

		THROW_ON_FAIL(ID3D12InfoQueue_PushStorageFilter(InfoQueue, &NewFilter));
	}
#endif // DEBUG

	ID3D12RootSignature* ComputeRootSignature;

	{
		D3D12_DESCRIPTOR_RANGE ranges[2] = { 0 };
		
		ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[0].NumDescriptors = 1;
		ranges[0].BaseShaderRegister = 0;
		ranges[0].RegisterSpace = 0;
		ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		
		ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		ranges[1].NumDescriptors = 1;
		ranges[1].BaseShaderRegister = 0;
		ranges[1].RegisterSpace = 0;
		ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		
		D3D12_ROOT_PARAMETER rootParameters[2] = { 0 };

		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[0].DescriptorTable.pDescriptorRanges = &ranges[0];
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[1].DescriptorTable.pDescriptorRanges = &ranges[1];
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		
		D3D12_ROOT_SIGNATURE_DESC computeRootSignatureDesc = { 0 };
		computeRootSignatureDesc.NumParameters = (UINT)(sizeof(rootParameters) / sizeof(rootParameters[0]));
		computeRootSignatureDesc.pParameters = rootParameters;
		computeRootSignatureDesc.NumStaticSamplers = 0;
		computeRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

		ID3DBlob* signature = nullptr;
		ID3DBlob* error = nullptr;
		THROW_ON_FAIL(D3D12SerializeRootSignature(&computeRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
		

		THROW_ON_FAIL(ID3D12Device_CreateRootSignature(
			Device,
			0,
			ID3D10Blob_GetBufferPointer(signature),
			ID3D10Blob_GetBufferSize(signature),
			&IID_ID3D12RootSignature,
			&ComputeRootSignature));
		
		if (signature)
		{
			ID3D10Blob_Release(signature);
		}
		if (error) 
		{
			ID3D10Blob_Release(error);
		}
	}

	THROW_ON_FAIL(ID3D12RootSignature_SetName(ComputeRootSignature, L"ComputeRootSignature"));

	ID3D12DescriptorHeap* DescriptorHeap;
	{
		D3D12_DESCRIPTOR_HEAP_DESC srvUavHeapDesc = { 0 };
		srvUavHeapDesc.NumDescriptors = 2;
		srvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		srvUavHeapDesc.NodeMask = 0;

		THROW_ON_FAIL(ID3D12Device_CreateDescriptorHeap(Device, &srvUavHeapDesc, &IID_ID3D12DescriptorHeap, (void**)&DescriptorHeap));
	}

	ID3D12DescriptorHeap_SetName(DescriptorHeap, L"DescriptorHeap");

	const size_t SrvUavDescriptorSize = ID3D12Device_GetDescriptorHandleIncrementSize(Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	HANDLE ComputeShaderFile = CreateFileW(
		L"compute.cso",
		GENERIC_READ,
		0,
		nullptr,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	VALIDATE_HANDLE(ComputeShaderFile);

	SIZE_T ComputeShaderSize;

	{
		LARGE_INTEGER tempLongInteger;

		THROW_ON_FALSE(GetFileSizeEx(ComputeShaderFile, &tempLongInteger));

		ComputeShaderSize = tempLongInteger.QuadPart;
	}

	HANDLE ComputeShaderFileMap = CreateFileMappingW(
		ComputeShaderFile,
		nullptr,
		PAGE_READONLY,
		0,
		0,
		nullptr);

	VALIDATE_HANDLE(ComputeShaderFileMap);

	void* ComputeShaderBytecode = MapViewOfFile(ComputeShaderFileMap, FILE_MAP_READ, 0, 0, 0);
	
	ID3D12PipelineState* ComputePipelineState;
	
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = { 0 };
		computePsoDesc.pRootSignature = ComputeRootSignature;
		computePsoDesc.CS.pShaderBytecode = ComputeShaderBytecode;
		computePsoDesc.CS.BytecodeLength = ComputeShaderSize;

		THROW_ON_FAIL(ID3D12Device_CreateComputePipelineState(Device, &computePsoDesc, &IID_ID3D12PipelineState, (void**)&ComputePipelineState));
	}

	ID3D12CommandQueue* ComputeCommandQueue;

	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = { 0 };
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Priority = 0;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 0;

		THROW_ON_FAIL(ID3D12Device_CreateCommandQueue(Device, &queueDesc, &IID_ID3D12CommandQueue, (void**)&ComputeCommandQueue));
	}

	ID3D12CommandAllocator* ComputeCmdAllocator;

	THROW_ON_FAIL(ID3D12Device_CreateCommandAllocator(Device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void**)&ComputeCmdAllocator));

	ID3D12GraphicsCommandList* ComputeCmdList;

	THROW_ON_FAIL(ID3D12Device_CreateCommandList(Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, ComputeCmdAllocator, nullptr, &IID_ID3D12CommandList, (void**)&ComputeCmdList));

	const uint32_t TEST_DATA_COUNT = 4096;

	uint32_t* inputBuffer = VirtualAlloc(
		nullptr,
		TEST_DATA_COUNT * 2 * sizeof(uint32_t),
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);

	uint32_t* resultBuffer = inputBuffer + TEST_DATA_COUNT;

	for (int i = 0; i < TEST_DATA_COUNT; i++)
	{
		inputBuffer[i] = i;
	}

	ID3D12Resource* SrcBufferResource;
	
	{
		D3D12_HEAP_PROPERTIES heapProperties = { 0 };
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC resourceDesc = { 0 };
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = sizeof(uint32_t) * TEST_DATA_COUNT;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(
			Device,
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr, &IID_ID3D12Resource,
			(void**)&SrcBufferResource));
	}

	ID3D12Resource* UploadBufferResource;

	{
		D3D12_HEAP_PROPERTIES heapUploadProperties = { 0 };
		heapUploadProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapUploadProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapUploadProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapUploadProperties.CreationNodeMask = 1;
		heapUploadProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC uploadBufferDesc = { 0 };
		uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uploadBufferDesc.Alignment = 0;
		uploadBufferDesc.Width = sizeof(uint32_t) * TEST_DATA_COUNT;
		uploadBufferDesc.Height = 1;
		uploadBufferDesc.DepthOrArraySize = 1;
		uploadBufferDesc.MipLevels = 1;
		uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		uploadBufferDesc.SampleDesc.Count = 1;
		uploadBufferDesc.SampleDesc.Quality = 0;
		uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(
			Device,
			&heapUploadProperties,
			D3D12_HEAP_FLAG_NONE,
			&uploadBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			&IID_ID3D12Resource,
			(void**)&UploadBufferResource));
	}

	{
		void* hostMemPtr;
		const D3D12_RANGE readRange = { 0, 0 };
		THROW_ON_FAIL(ID3D12Resource_Map(UploadBufferResource, 0, &readRange, &hostMemPtr));

		memcpy(hostMemPtr, inputBuffer, sizeof(uint32_t) * TEST_DATA_COUNT);
		ID3D12Resource_Unmap(UploadBufferResource, 0, nullptr);
	}

	{
		D3D12_RESOURCE_BARRIER beginCopyBarrier = { 0 };
		beginCopyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		beginCopyBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		beginCopyBarrier.Transition.pResource = SrcBufferResource;
		beginCopyBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		beginCopyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		beginCopyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

		ID3D12GraphicsCommandList_ResourceBarrier(ComputeCmdList, 1, &beginCopyBarrier);
	}

	ID3D12GraphicsCommandList_CopyBufferRegion(ComputeCmdList, SrcBufferResource, 0, UploadBufferResource, 0, sizeof(uint32_t) * TEST_DATA_COUNT);

	{
		D3D12_RESOURCE_BARRIER endCopyBarrier = { 0 };
		endCopyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		endCopyBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		endCopyBarrier.Transition.pResource = SrcBufferResource;
		endCopyBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		endCopyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		endCopyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

		ID3D12GraphicsCommandList_ResourceBarrier(ComputeCmdList, 1, &endCopyBarrier);
	}

	{
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = { 0 };
		ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(DescriptorHeap, &srvHandle);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { 0 };
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = TEST_DATA_COUNT;
		srvDesc.Buffer.StructureByteStride = sizeof(uint32_t);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		ID3D12Device_CreateShaderResourceView(Device, SrcBufferResource, &srvDesc, srvHandle);
	}

	ID3D12Resource* DstBufferResource;

	{
		D3D12_HEAP_PROPERTIES heapProperties = { 0 };
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC resourceDesc = { 0 };
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = sizeof(uint32_t) * TEST_DATA_COUNT;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(
			Device,
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			&IID_ID3D12Resource,
			(void**)&DstBufferResource));
	}

	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = { 0 };
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = TEST_DATA_COUNT;
		uavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		
		D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = { 0 };
		ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(DescriptorHeap, &uavHandle);

		uavHandle.ptr += 1 * SrvUavDescriptorSize;

		ID3D12Device_CreateUnorderedAccessView(Device, DstBufferResource, nullptr, &uavDesc, uavHandle);
	}

	ID3D12Fence* Fence;

	THROW_ON_FAIL(ID3D12Device_CreateFence(Device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&Fence));

	HANDLE EventHandle;

	VALIDATE_HANDLE(EventHandle = CreateEventA(nullptr, FALSE, FALSE, nullptr));	
	
	THROW_ON_FAIL(ID3D12GraphicsCommandList_Close(ComputeCmdList));

	ID3D12CommandQueue_ExecuteCommandLists(ComputeCommandQueue, 1, (ID3D12CommandList* const []) { (ID3D12CommandList*)ComputeCmdList });

	THROW_ON_FAIL(ID3D12CommandQueue_Signal(ComputeCommandQueue, Fence, UPLOAD_DATA_SIGNAL_VALUE));
	
	THROW_ON_FAIL(ID3D12Fence_SetEventOnCompletion(Fence, UPLOAD_DATA_SIGNAL_VALUE, EventHandle));
	
	WaitForSingleObject(EventHandle, INFINITE);
	
	ID3D12Resource_Release(UploadBufferResource);

	ID3D12Resource* readBackBuffer;

	{
		D3D12_HEAP_PROPERTIES heapProperties = { 0 };
		heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC resourceDesc = { 0 };
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = sizeof(uint32_t) * TEST_DATA_COUNT;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		THROW_ON_FAIL(ID3D12Device_CreateCommittedResource(Device,
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			&IID_ID3D12Resource,
			(void**)&readBackBuffer));
	}

	THROW_ON_FAIL(ID3D12CommandAllocator_Reset(ComputeCmdAllocator));
	
	THROW_ON_FAIL(ID3D12GraphicsCommandList_Reset(ComputeCmdList, ComputeCmdAllocator, ComputePipelineState));
		
	ID3D12GraphicsCommandList_SetComputeRootSignature(ComputeCmdList, ComputeRootSignature);

	{
		ID3D12DescriptorHeap* const ppHeaps[] = { DescriptorHeap };
		ID3D12GraphicsCommandList_SetDescriptorHeaps(ComputeCmdList, (UINT)(sizeof(ppHeaps) / sizeof(ppHeaps[0])), ppHeaps);
	}

	{
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = { 0 };

		ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(DescriptorHeap, &srvHandle);

		D3D12_GPU_DESCRIPTOR_HANDLE uavHandle = { 0 };

		ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(DescriptorHeap, &uavHandle);

		uavHandle.ptr += 1 * SrvUavDescriptorSize;

		ID3D12GraphicsCommandList_SetComputeRootDescriptorTable(ComputeCmdList, 0, srvHandle);
		ID3D12GraphicsCommandList_SetComputeRootDescriptorTable(ComputeCmdList, 1, uavHandle);
	}

	ID3D12GraphicsCommandList_Dispatch(ComputeCmdList, 4, 1, 1);

	{
		D3D12_RESOURCE_BARRIER beginCopyBarrier = { 0 };
		beginCopyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		beginCopyBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		beginCopyBarrier.Transition.pResource = DstBufferResource;
		beginCopyBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		beginCopyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		beginCopyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

		ID3D12GraphicsCommandList_ResourceBarrier(ComputeCmdList, 1, &beginCopyBarrier);
	}

	ID3D12GraphicsCommandList_CopyResource(ComputeCmdList, readBackBuffer, DstBufferResource);

	{
		D3D12_RESOURCE_BARRIER endCopyBarrier = { 0 };
		endCopyBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		endCopyBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		endCopyBarrier.Transition.pResource = DstBufferResource;
		endCopyBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		endCopyBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		endCopyBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		ID3D12GraphicsCommandList_ResourceBarrier(ComputeCmdList, 1, &endCopyBarrier);
	}
		
	ID3D12GraphicsCommandList_Close(ComputeCmdList);

	ID3D12CommandQueue_ExecuteCommandLists(ComputeCommandQueue, 1, (ID3D12CommandList * []) { (ID3D12CommandList*)ComputeCmdList });

	THROW_ON_FAIL(ID3D12CommandQueue_Signal(ComputeCommandQueue, Fence, COMPUTE_EXECUTED_SIGNAL_VALUE));

	THROW_ON_FAIL(ID3D12Fence_SetEventOnCompletion(Fence, COMPUTE_EXECUTED_SIGNAL_VALUE, EventHandle));

	WaitForSingleObject(EventHandle, INFINITE);
	
	{
		void* pData;
		const D3D12_RANGE range = { 0, TEST_DATA_COUNT };

		THROW_ON_FAIL(ID3D12Resource_Map(readBackBuffer, 0, &range, &pData));

		memcpy(resultBuffer, pData, sizeof(uint32_t) * TEST_DATA_COUNT);

		ID3D12Resource_Unmap(readBackBuffer, 0, nullptr);
		ID3D12Resource_Release(readBackBuffer);

		bool equal = true;
		for (int i = 0; i < TEST_DATA_COUNT; i++)
		{
			if (resultBuffer[i] - 10 != inputBuffer[i])
			{
				printf("%d index elements are not equal! (%i)\n", i, resultBuffer[i]);
				equal = false;
				break;
			}
		}
		if (equal)
		{
			puts("Verification OK!");
		}
	}
	
	CloseHandle(EventHandle);
	ID3D12Fence_Release(Fence);
	ID3D12DescriptorHeap_Release(DescriptorHeap);
	ID3D12Resource_Release(SrcBufferResource);
	ID3D12Resource_Release(DstBufferResource);
	ID3D12CommandAllocator_Release(ComputeCmdAllocator);
	ID3D12GraphicsCommandList_Release(ComputeCmdList);
	ID3D12CommandQueue_Release(ComputeCommandQueue);
	ID3D12PipelineState_Release(ComputePipelineState);
	ID3D12RootSignature_Release(ComputeRootSignature);
	ID3D12Device_Release(Device);
	IDXGIFactory4_Release(Factory);
}