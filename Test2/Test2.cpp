
#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include "resource.h"

#define MX_SETWORLD 0x101

//--------------------------------------------------------------------------------------
// Структуры
//--------------------------------------------------------------------------------------

struct SimpleVertex
{
    XMFLOAT3 Pos;       // Координаты точки в пространстве
    XMFLOAT3 Normal;    // Нормаль вершины
};

// Структура константного буфера (совпадает со структурой в шейдере)

struct ConstantBuffer
{
    XMMATRIX mWorld;       // Матрица мира
    XMMATRIX mView;        // Матрица вида
    XMMATRIX mProjection;  // Матрица проекции

    XMFLOAT4 vLightDir[2];  // Направление источника света
    XMFLOAT4 vLightColor[2];// Цвет источниа цвета
    XMFLOAT4 vOutputColor;  // Активный цвет (для второго PSSolid)
};

//--------------------------------------------------------------------------------------
// Глобальные переменные
//--------------------------------------------------------------------------------------

HINSTANCE               g_hInst = NULL;
HWND                    g_hWnd = NULL;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;        // Версия поддержки DirectX 
ID3D11Device*           g_pd3dDevice = NULL;                            // Устройство (для создания объектов)
ID3D11DeviceContext*    g_pImmediateContext = NULL;                     // Контекст устройства (рисование)
IDXGISwapChain*         g_pSwapChain = NULL;                            // Цепь связи (буфера с экраном)
ID3D11RenderTargetView* g_pRenderTargetView = NULL;                     // Объект заднего буфера
ID3D11Texture2D*        g_pDepthStencil = NULL;                         // Текстура буфера глубин
ID3D11DepthStencilView* g_pDepthStencilView = NULL;                     // Объект вида, буфер глубин

ID3D11VertexShader*     g_pVertexShader = NULL;                         // Вершинный шейдер
ID3D11PixelShader*      g_pPixelShader = NULL;                          // Пиксельный шейдер для куба
ID3D11PixelShader*      g_pPixelShaderSolid = NULL;                     // Пиксельный шейдер для источников света
ID3D11InputLayout*      g_pVertexLayout = NULL;                         // Описание формата вершин
ID3D11Buffer*           g_pVertexBuffer = NULL;                         // Буфер вершин
ID3D11Buffer*           g_pIndexBuffer = NULL;                          // Буфер индексов вершин
ID3D11Buffer*           g_pConstantBuffer = NULL;                       // Константный буфер

XMMATRIX                g_World;                                        // Матрица мира
XMMATRIX                g_View;                                         // Матрица вида
XMMATRIX                g_Projection;                                   // Матрица проекции
FLOAT                   t = 0.0f;                                       // Переменная времени

XMFLOAT4                vLightDirs[2];                                  // направление света ( позиция источников света)
XMFLOAT4                vLightColors[2];                                // Цвет источников

//--------------------------------------------------------------------------------------
// Предварительные объявления функций
//--------------------------------------------------------------------------------------

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);  // Создание окна
HRESULT InitDevice();                                   // Инициализация устройств DirectX
HRESULT InitGeometry();                                 // Инициализация шаблона ввода и буфера вершин
HRESULT InitMatrixes();  
void UpdateLight();                                     // Обновление параметров света
void UpdateMatrix(UINT nLightIndex);                    // Обновление матрицы мира
void Render();                                          // Функция рисования
void CleanupDevice();                                   // Удаление созданнных устройств DirectX
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);   // Функция окна

//--------------------------------------------------------------------------------------
// Точка входа в программу. Здесь мы все инициализируем и входим в цикл сообщений.
// Время простоя используем для вызова функции рисования
//--------------------------------------------------------------------------------------

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Создание окна приложения
    if (FAILED(InitWindow(hInstance, nCmdShow)))
        return 0;

    // Создание объектов DirectX
    if (FAILED(InitDevice()))
    {
        CleanupDevice();
        return 0;
    }

    // Создание шейдеров и буфера вершин
    if (FAILED(InitGeometry()))
    {
        CleanupDevice();
        return 0;
    }

    // Инициализация матриц
    if (FAILED(InitMatrixes()))
    {
        CleanupDevice();
        return 0;
    }

    // Главный цикл сообщений
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else // Если сообщений нет
        {
            Render();      // Рисуем
        }
    }
    CleanupDevice();

    return (int)msg.wParam;
}

//--------------------------------------------------------------------------------------
// Регистрация класса и создание окна
//--------------------------------------------------------------------------------------

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    // Регистрация класса
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"Test1WindowClass";
    wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_ICON1);
    if (!RegisterClassEx(&wcex))
        return E_FAIL;

    // Создание окна
    g_hInst = hInstance;
    RECT rc = { 0, 0, 640, 480 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindow(L"Test1WindowClass", L"Тест 2",
                        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT, CW_USEDEFAULT, 
                        rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
                        NULL);
    if (!g_hWnd)
        return E_FAIL;

    ShowWindow(g_hWnd, nCmdShow);
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Вызывается каждый раз, когда приложение получает сообщение
//--------------------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    switch (message)
    {
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

//--------------------------------------------------------------------------------------
// Вспомогательная функция для компиляции шейдеров в D3DX11
//--------------------------------------------------------------------------------------

HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
    HRESULT hr = S_OK;
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
        dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL);
    if (FAILED(hr))
    {
        if (pErrorBlob != NULL)
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
        if (pErrorBlob) pErrorBlob->Release();
        return hr;
    }
    if (pErrorBlob) pErrorBlob->Release();
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Создание буфера вершин, шейдеров (shaders) и описания формата вершин (input layout)
//--------------------------------------------------------------------------------------

HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(g_hWnd, &rc);         // получаем координаты нашего окна  
    UINT width = rc.right - rc.left;    // получаем ширину
    UINT height = rc.bottom - rc.top;   // и высоту окна

    UINT createDeviceFlags = 0;

#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    // Тут мы создаем список поддерживаемых версий DirectX

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    // Сейчас мы создадим устройства DirectX. Для начала заполним структуру,
    // которая описывает свойства переднего буфера и привязывает его к нашему окну.
    DXGI_SWAP_CHAIN_DESC sd;             // Структура, описывающая цепь связи (Swap Chain)
    ZeroMemory(&sd, sizeof(sd));         // очищаем ее
    sd.BufferCount = 1;                                     // у нас один задний буфер
    sd.BufferDesc.Width = width;                            // ширина буфера
    sd.BufferDesc.Height = height;                          // высота буфера
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;      // формат пикселя в буфере
    sd.BufferDesc.RefreshRate.Numerator = 75;               // частота обновления экрана
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;       // назначение буфера - задний буфер
    sd.OutputWindow = g_hWnd;                               // привязываем к нашему окну
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;                                     // не полноэкранный режим

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDeviceAndSwapChain(NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
        
        if (SUCCEEDED(hr)) // Если устройства созданы успешно, то выходим из цикла
            break;
    }

    if (FAILED(hr)) return hr;

    // Теперь создаем задний буфер.

    // RenderTargetOutput - это передний буфер, а RenderTargetView - задний.
    // Извлекаем описание заднего буфера
    ID3D11Texture2D* pBackBuffer = NULL;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer); // загружает характеристики буфера
    if (FAILED(hr)) return hr;

    // По полученному описанию создаем поверхность рисования
    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr)) return hr;

    // Переходим к созданию буфера глубин
    // Создаем текстуру-описание буфера глубин
    D3D11_TEXTURE2D_DESC descDepth;                         // Структура с параметрами
    ZeroMemory(&descDepth, sizeof(descDepth));
    descDepth.Width = width;                                // ширина и
    descDepth.Height = height;                              // высота текстуры
    descDepth.MipLevels = 1;                                // уровень интерполяции
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;       // формат (размер пикселя)
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;         // вид - буфер глубин
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;

    // При помощи заполненной структуры-описания создаем объект текстуры
    hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
    if (FAILED(hr)) return hr;

    // Теперь надо создать сам объект буфера глубин
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;                  // Структура с параметрами
    ZeroMemory(&descDSV, sizeof(descDSV));
    descDSV.Format = descDepth.Format;                      // формат как в текстуре
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;

    // При помощи заполненной структуры-описания и текстуры создаем объект буфера глубин
    hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
    if (FAILED(hr)) return hr;

    // Подключаем объект заднего буфера к контексту устройства
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

    // Настройка вьюпорта
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;             // Минимальная глубина области просмотра. 
    vp.MaxDepth = 1.0f;             // Максимальная глубина области просмотра.
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;

    // Подключаем вьюпорт к контексту устройства
    g_pImmediateContext->RSSetViewports(1, &vp);

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Освобождение всех созданных объектов
//--------------------------------------------------------------------------------------

void CleanupDevice()
{
    // Сначала отключим контекст устройства
    if (g_pImmediateContext) g_pImmediateContext->ClearState();
    // Потом удалим объекты
    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pIndexBuffer) g_pIndexBuffer->Release();
    if (g_pVertexLayout) g_pVertexLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pDepthStencil) g_pDepthStencil->Release();
    if (g_pDepthStencilView) g_pDepthStencilView->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
}

//--------------------------------------------------------------------------------------
// Инициализация матриц
//--------------------------------------------------------------------------------------

HRESULT InitMatrixes()
{
    RECT rc;
    GetClientRect(g_hWnd, &rc);
    UINT width = rc.right - rc.left;            // получаем ширину
    UINT height = rc.bottom - rc.top;           // и высоту окна

    // Инициализация матрицы мира
    g_World = XMMatrixIdentity();

    // Инициализация матрицы вида
    XMVECTOR Eye = XMVectorSet(0.0f, 4.0f, -10.0f, 0.0f);  // Откуда смотрим
    XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);    // Куда смотрим
    XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);    // Направление верха
    g_View = XMMatrixLookAtLH(Eye, At, Up);

    // Инициализация матрицы проекции
    g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT)height, 0.01f, 100.0f);
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Обновление матриц
//----------------------------------------W----------------------------------------------

void SetMatrixes(float fAngle)
{
    // Обновление переменной-времени
    static float t = 0.0f;
    if (g_driverType == D3D_DRIVER_TYPE_REFERENCE)
    {
        t += (float)XM_PI * 0.0125f;
    }
    else
    {
        static DWORD dwTimeStart = 0;
        DWORD dwTimeCur = GetTickCount();
        if (dwTimeStart == 0)
            dwTimeStart = dwTimeCur;
        t = (dwTimeCur - dwTimeStart) / 1000.0f;
    }

    // Матрица-орбита: позиция объекта
    XMMATRIX mOrbit = XMMatrixRotationY(-t + fAngle);
    // Матрица-спин: вращение объекта вокруг своей оси
    XMMATRIX mSpin = XMMatrixRotationY(t * 2);
    // Матрица-позиция: перемещение на три единицы влево от начала координат
    XMMATRIX mTranslate = XMMatrixTranslation(-3.0f, 0.0f, 0.0f);
    // Матрица-масштаб: сжатие объекта в 2 раза
    XMMATRIX mScale = XMMatrixScaling(0.5f, 0.5f, 0.5f);

    // Результирующая матрица
    //  --Сначала мы в центре, в масштабе 1:1:1, повернуты по всем осям на 0.0f.
    //  --Сжимаем -> поворачиваем вокруг Y (пока мы еще в центре) -> переносим влево ->
    //  --снова поворачиваем вокруг Y.
    g_World = mScale * mSpin * mTranslate * mOrbit;

    // Обновить константный буфер
    // создаем временную структуру и загружаем в нее матрицы
    ConstantBuffer cb;
    cb.mWorld = XMMatrixTranspose(g_World);
    cb.mView = XMMatrixTranspose(g_View);
    cb.mProjection = XMMatrixTranspose(g_Projection);

    // загружаем временную структуру в константный буфер g_pConstantBuffer
    g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, NULL, &cb, 0, 0);
}

HRESULT InitGeometry()
{
    HRESULT hr = S_OK;

    // Компиляция вершинного шейдера из файла
    ID3DBlob* pVSBlob = NULL; // Вспомогательный объект - просто место в оперативной памяти
    hr = CompileShaderFromFile(L"test2.fx", "VS", "vs_4_0", &pVSBlob);
    if (FAILED(hr))
    {
        MessageBox(NULL, L"Невозможно скомпилировать файл FX. Пожалуйста, запустите данную программу из папки, содержащей файл VS.", L"Ошибка", MB_OK);
        return hr;
    }

    // Создание вершинного шейдера
    hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader);
    if (FAILED(hr))
    {
        pVSBlob->Release();
        return hr;
    }

    // Определение шаблона вершин
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE(layout);

    // Создание шаблона вершин
    hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(), &g_pVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr)) return hr;

    // Подключение шаблона вершин
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

    // Компиляция пиксельного шейдера для основного большого куба из файла
    ID3DBlob* pPSBlob = NULL;
    hr = CompileShaderFromFile(L"test2.fx", "PS", "ps_4_0", &pPSBlob);
    if (FAILED(hr))
    {
        MessageBox(NULL, L"Невозможно скомпилировать файл FX. Пожалуйста, запустите данную программу из папки, содержащей файл FX.", L"Ошибка", MB_OK);
        return hr;
    }

    // Создание пиксельного шейдера
    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader);
    pPSBlob->Release();
    if (FAILED(hr)) return hr;

    // Компиляция пиксельного шейдера для источников света из файла
    pPSBlob = NULL;
    hr = CompileShaderFromFile(L"test2.fx", "PSSolid", "ps_4_0", &pPSBlob);
    if (FAILED(hr))
    {
        MessageBox(NULL, L"Невозможно скомпилировать файл FX. Пожалуйста, запустите данную программу из папки, содержащей файл FX.", L"Ошибка", MB_OK);
        return hr;
    }

    // Создание пиксельного шейдера
    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShaderSolid);
    pPSBlob->Release();
    if (FAILED(hr)) return hr;

    // Создание буфера вершин (по 4 точки на каждую сторону куба, всего 24 вершины)
    SimpleVertex vertices[] =
    {  /* координаты X, Y, Z                          нормаль X, Y, Z     */
        { XMFLOAT3(-1.0f, 1.0f, -1.0f),      XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },

        { XMFLOAT3(-1.0f, -1.0f, -1.0f),     XMFLOAT3(0.0f, -1.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f),      XMFLOAT3(0.0f, -1.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
        { XMFLOAT3(-1.0f, -1.0f, 1.0f),      XMFLOAT3(0.0f, -1.0f, 0.0f) },

        { XMFLOAT3(-1.0f, -1.0f, 1.0f),      XMFLOAT3(-1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(-1.0f, -1.0f, -1.0f),     XMFLOAT3(-1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, -1.0f),      XMFLOAT3(-1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },

        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f),      XMFLOAT3(1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },

        { XMFLOAT3(-1.0f, -1.0f, -1.0f),     XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f),      XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3(-1.0f, 1.0f, -1.0f),      XMFLOAT3(0.0f, 0.0f, -1.0f) },

        { XMFLOAT3(-1.0f, -1.0f, 1.0f),      XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC bd;	// Структура, описывающая создаваемый буфер
    ZeroMemory(&bd, sizeof(bd));				// очищаем ее
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex) * 24;	// размер буфера
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// тип буфера - буфер вершин
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;	// Структура, содержащая данные буфера
    ZeroMemory(&InitData, sizeof(InitData));	// очищаем ее
    InitData.pSysMem = vertices;				// указатель на наши 8 вершин
    // Вызов метода g_pd3dDevice создаст объект буфера вершин
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
    if (FAILED(hr)) return hr;

    // Создание буфера индексов:
    // Создание массива с данными
    WORD indices[] =
    {
        3,1,0,
        2,1,3,

        6,4,5,
        7,4,6,

        11,9,8,
        10,9,11,

        14,12,13,
        15,12,14,

        19,17,16,
        18,17,19,

        22,20,21,
        23,20,22
    };

    bd.Usage = D3D11_USAGE_DEFAULT;		    // Структура, описывающая создаваемый буфер
    bd.ByteWidth = sizeof(WORD) * 36;       // 36 вершин для 12 треугольников (6 сторон)
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER; // тип - буфер индексов
    bd.CPUAccessFlags = 0;
    InitData.pSysMem = indices;				// указатель на наш массив индексов
    // Вызов метода g_pd3dDevice создаст объект буфера индексов
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
    if (FAILED(hr)) return hr;

    // Установка буфера вершин
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    // Установка буфера индексов
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    // Установка способа отрисовки вершин в буфере (в данном случае - TRIANGLE LIST,
    // т. е. точки 1-3 - первый треугольник, 4-6 - второй и т. д.
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Создание константного буфера
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ConstantBuffer);		// размер буфера = размеру структуры
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;	// тип - константный буфер
    bd.CPUAccessFlags = 0;
    // Вызов метода g_pd3dDevice создаст объект константного буфера
    hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pConstantBuffer);
    if (FAILED(hr)) return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Вычисляем направление света
//--------------------------------------------------------------------------------------

void UpdateLight()
{
    // Обновление переменной-времени
    if (g_driverType == D3D_DRIVER_TYPE_REFERENCE)
    {
        t += (float)XM_PI * 0.0125f;
    }
    else
    {
        static DWORD dwTimeStart = 0;
        DWORD dwTimeCur = GetTickCount();
        if (dwTimeStart == 0)
            dwTimeStart = dwTimeCur;
        t = (dwTimeCur - dwTimeStart) / 1000.0f;
    }

    // Задаем начальные координаты источников света
    vLightDirs[0] = XMFLOAT4(-0.577f, 0.577f, -0.577f, 1.0f);
    vLightDirs[1] = XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f);

    // Задаем цвет источников света, у нас он не будет меняться
    vLightColors[0] = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vLightColors[1] = XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f);

    // При помощи трансформаций поворачиваем второй источник света
    XMMATRIX mRotate = XMMatrixRotationY(-2.0f * t);
    XMVECTOR vLightDir = XMLoadFloat4(&vLightDirs[1]);
    vLightDir = XMVector3Transform(vLightDir, mRotate);
    XMStoreFloat4(&vLightDirs[1], vLightDir);

    // При помощи трансформаций поворачиваем первый источник света
    mRotate = XMMatrixRotationY(0.5f * t);
    vLightDir = XMLoadFloat4(&vLightDirs[0]);
    vLightDir = XMVector3Transform(vLightDir, mRotate);
    XMStoreFloat4(&vLightDirs[0], vLightDir);
}

//--------------------------------------------------------------------------------------
// Устанавливаем матрицы для текущего источника света (0-1) или мира (MX_SETWORLD)
//--------------------------------------------------------------------------------------

void UpdateMatrix(UINT nLightIndex)
{
    // Небольшая проверка индекса
    if (nLightIndex == MX_SETWORLD) {
        // Если рисуем центральный куб: его надо просто вращать
        g_World = XMMatrixRotationAxis(XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f), t);
        nLightIndex = 0;
    }
    else if (nLightIndex < 2) {

        // Если рисуем источники света: перемещаем матрицу в точку и уменьшаем в 5 раз
        g_World = XMMatrixTranslationFromVector(5.0f * XMLoadFloat4(&vLightDirs[nLightIndex]));
        XMMATRIX mLightScale = XMMatrixScaling(0.2f, 0.2f, 0.2f);
        g_World = mLightScale * g_World;
    }
    else {
        nLightIndex = 0;
    }

    // Обновление содержимого константного буфера
    ConstantBuffer cb1;    // временный контейнер
    cb1.mWorld = XMMatrixTranspose(g_World); // загружаем в него матрицы
    cb1.mView = XMMatrixTranspose(g_View);
    cb1.mProjection = XMMatrixTranspose(g_Projection);
    cb1.vLightDir[0] = vLightDirs[0];          // загружаем данные о свете
    cb1.vLightDir[1] = vLightDirs[1];
    cb1.vLightColor[0] = vLightColors[0];
    cb1.vLightColor[1] = vLightColors[1];
    cb1.vOutputColor = vLightColors[nLightIndex];
    g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, NULL, &cb1, 0, 0);
}

//--------------------------------------------------------------------------------------
// Рисование кадра
//--------------------------------------------------------------------------------------

void Render()
{
    // Очистка заднего буфера
    float ClearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f }; // красный, зеленый, синий, альфа-канал
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);

    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    UpdateLight();

    UpdateMatrix(MX_SETWORLD);
    g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
    g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);
    g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
    g_pImmediateContext->DrawIndexed(36, 0, 0);

    g_pImmediateContext->PSSetShader(g_pPixelShaderSolid, NULL, 0);
    for (int m = 0; m < 2; m++)
    {
        UpdateMatrix(m);
        g_pImmediateContext->DrawIndexed(36, 0, 0);
    }

    g_pSwapChain->Present(0, 0);


}



