// coscon.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include <d3d12.h>

#include <stdio.h>

#include <exception>
#include <memory>
#include <list>


template<class T> class D3DPointer
{
public:

    D3DPointer() : m_p(NULL) {}
    ~D3DPointer() { if (m_p) m_p->Release(); }

    T * operator->() { return m_p; }

    operator bool() { return m_p != NULL; }
    operator T *() { return m_p; }

    D3DPointer<T> & operator=(T * p) { if (m_p) m_p->Release();  m_p = p; return *this; }

private:

    T * m_p;

};


class D3DDevice
{
public:

    D3DDevice(std::wstring & driverString)
    {
        if (!FindAdapter(driverString)) throw std::exception("Failed to find adapter");
        if (!CreateDevice()) throw std::exception("Failed to create device");
    }

    ~D3DDevice()
    {
        // do nothing
    }

    ID3D12Device * GetDevice() { return m_pDevice; }

private:

    D3DPointer<IDXGIAdapter2>               m_pAdapter;
    D3DPointer<ID3D12Device>                m_pDevice;

    bool CreateDevice()
    {
        ID3D12Device * pDevice;

        HRESULT hr = D3D12CreateDevice(m_pAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice));

        bool success = (hr == S_OK);

        if (success)
            m_pDevice = pDevice;

        return success;
    }

    bool FindAdapter(std::wstring & driverString)
    {
        bool found = false;
        IDXGIFactory2 * factory = NULL;
        HRESULT hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory2), ((void **)&factory));
        if (FAILED(hr)) throw std::exception("Unable to create DXGIFactor2");

        UINT adapterIndex = 0;
        bool done = false;

        while (!done && !found) {

            IDXGIAdapter1 * adapter = NULL;

            hr = factory->EnumAdapters1(adapterIndex, &adapter);

            if (hr == S_OK)
            {
                IDXGIAdapter2 * adapter2 = NULL;

                hr = adapter->QueryInterface(__uuidof(IDXGIAdapter2), (void **)&adapter2);

                if (hr == S_OK)
                {
                    DXGI_ADAPTER_DESC2 desc;
                    adapter2->GetDesc2(&desc);

                    found = (wcscmp(driverString.c_str(), desc.Description) == 0);

                    if (found)
                    {
                        m_pAdapter = adapter2;
                    }
                    else
                    {
                        adapter2->Release();
                    }
                }

                adapter->Release();

                adapterIndex++;
            }
            else
            {
                done = true;
            }
        }

        factory->Release();

        return found;
    }

};


class D3DAdapter
{
public:

    static void GetAdapterList(std::list<std::wstring> & list)
    {
        IDXGIFactory2 * factory = NULL;
        HRESULT hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory2), ((void **)&factory));
        if (FAILED(hr)) throw std::exception("Unable to create DXGIFactor2");

        UINT adapterIndex = 0;
        bool done = false;

        list.clear();

        while (!done) {

            IDXGIAdapter1 * adapter = NULL;

            hr = factory->EnumAdapters1(adapterIndex, &adapter);

            if (hr == S_OK)
            {
                IDXGIAdapter2 * adapter2 = NULL;

                hr = adapter->QueryInterface(__uuidof(IDXGIAdapter2), (void **)&adapter2);

                if (hr == S_OK)
                {
                    DXGI_ADAPTER_DESC2 desc;
                    adapter2->GetDesc2(&desc);

                    list.push_back(desc.Description);

                    adapter2->Release();
                }

                adapter->Release();

                adapterIndex++;
            }
            else
            {
                done = true;
            }
        }

        factory->Release();
    }
};


class D3DBuffer {

public:
    D3DBuffer(D3DDevice & inDevice, UINT uSize, D3D12_HEAP_TYPE inHeapType = D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATES inInitialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAGS inResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    {
        D3D12_HEAP_PROPERTIES heapProperties;

        heapProperties.Type = inHeapType;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 0;
        heapProperties.VisibleNodeMask = 0;

        D3D12_RESOURCE_DESC resourceDesc;

        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = uSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = inResourceFlags;

        ID3D12Resource * pBuffer;

        OutputDebugStringA("creating structured buffer\n");

        HRESULT hr = inDevice.GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
            inInitialState, NULL, IID_PPV_ARGS(&pBuffer));

        if (FAILED(hr)) throw std::exception("Unable to create structed buffer");

        m_pBuffer = pBuffer;
        m_initialState = inInitialState;
        m_currentState = inInitialState;
    }

    void StartStateTracking()
    {
        m_currentState = m_initialState;
    }

    ID3D12Resource * Get() { return m_pBuffer; }

protected:
    D3DPointer<ID3D12Resource>  m_pBuffer;
    D3D12_RESOURCE_STATES       m_initialState;
    D3D12_RESOURCE_STATES       m_currentState;
};


struct BufType
{
    int i;
    float f;
};

#define NUM_ELEMENTS     1024*64


int main()
{
    int dbgFlags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);

    dbgFlags |= _CRTDBG_CHECK_ALWAYS_DF;

    _CrtSetDbgFlag(dbgFlags);
    std::wstring cosDriverString = L"Compute Only Sample Driver";
    std::wstring brdDriverString = L"Microsoft Basic Render Driver";
    std::wstring amdDriverString = L"Radeon (TM) RX 480 Graphics";
    std::wstring intelDriverString = L"Intel(R) HD Graphics 620";

    std::list<std::wstring> adapterList;

    D3DAdapter::GetAdapterList(adapterList);

    printf("Found adapters:\n");
    for (std::wstring & a : adapterList)
        printf("  %S\n", a.c_str());

    auto findIter = std::find(adapterList.begin(), adapterList.end(), brdDriverString);

    if (findIter == adapterList.end()) {
        printf("%S was not found\n", cosDriverString.c_str());
        return 0;
    }

    try {

        std::wstring driverString = cosDriverString;

        findIter = std::find(adapterList.begin(), adapterList.end(), driverString);

        if (findIter == adapterList.end())
            driverString = brdDriverString;

        printf("Creating device on %S ... ", driverString.c_str());
        D3DDevice computeDevice(driverString);
        printf("done.\n");

        printf("Creating buffers ... ");

        //
        // Create a 512 KB buffer
        //

        UINT uSize = sizeof(BufType) * NUM_ELEMENTS;

        D3DBuffer a(computeDevice, uSize);

        printf("done.\n");

        a.StartStateTracking();

        ID3D12Pageable * pIPageable;

        a.Get()->QueryInterface(__uuidof(ID3D12Pageable), (void **)&pIPageable);

        //
        // D3D12 resource is created and then immediately made resident
        //
        // To really exercise the KMD's paging support, this test needs the UMD
        // to override EvictOnlyIfNecessary flag with 0 in its pfnEvict DDI
        //

        computeDevice.GetDevice()->Evict(1, &pIPageable);
        
        computeDevice.GetDevice()->MakeResident(1, &pIPageable);
    }
    catch (std::exception & e)
    {
        printf("Hit error: %s\n", e.what());
    }

    printf("Done.\n");

    return 0;
}

