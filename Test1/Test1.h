#pragma once

#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include "resource.h" 
#include <stdarg.h>


//--------------------------------------------------------------------------------------
// Глобальные переменные
//--------------------------------------------------------------------------------------

HINSTANCE               g_hInst = NULL;
HWND                    g_hWnd = NULL;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*           g_pd3dDevice = NULL;          // Устройство (для создания объектов)
ID3D11DeviceContext*    g_pImmediateContext = NULL;   // Контекст устройства (рисование)
IDXGISwapChain*         g_pSwapChain = NULL;          // Цепь связи (буфера с экраном)
ID3D11RenderTargetView* g_pRenderTargetView = NULL;   // Объект заднего буфера

//--------------------------------------------------------------------------------------
// Предварительные объявления функций
//--------------------------------------------------------------------------------------

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);  // Создание окна
HRESULT InitDevice();      // Инициализация устройств DirectX
void CleanupDevice();      // Удаление созданнных устройств DirectX
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);       // Функция окна
void Render();      // Функция рисования

//--------------------------------------------------------------------------------------
// Точка входа в программу. Здесь мы все инициализируем и входим в цикл сообщений.
// Время простоя используем для вызова функции рисования
//--------------------------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    if (FAILED(InitWindow(hInstance, nCmdShow)))
        return 0;
    if (FAILED(InitDevice()))
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
    // Регистрация 
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
    wcex.lpszClassName = L"Urok01WindowClass";
    wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_ICON1);
    if (!RegisterClassEx(&wcex))
        return E_FAIL;

    // Создание окна
    g_hInst = hInstance;
    RECT rc = { 0, 0, 640, 480 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindow(L"Urok01WindowClass", L"Урок 1: Создание устройств Direct3D", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance, NULL);
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

HRESULT InitDevice()
{
    HRESULT hr = S_OK;
    RECT rc;
    GetClientRect(g_hWnd, &rc);         // получаем координаты нашего окна  
    UINT width = rc.right - rc.left;    // получаем ширину
    UINT height = rc.bottom - rc.top;   // и высоту окна
    UINT createDeviceFlags = 0;

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
    DXGI_SWAP_CHAIN_DESC sd;            // Структура, описывающая цепь связи (Swap Chain)
    ZeroMemory(&sd, sizeof(sd));    // очищаем ее
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

    // Теперь создаем задний буфер. Обратите внимание, в SDK
    // RenderTargetOutput - это передний буфер, а RenderTargetView - задний.
    ID3D11Texture2D* pBackBuffer = NULL;
    hr = g_pSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer ); // загружает характеристики буфера
    if (FAILED(hr)) return hr;

    // Я уже упоминал, что интерфейс g_pd3dDevice будет
    // использоваться для создания остальных объектов
    hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, NULL, &g_pRenderTargetView );
    pBackBuffer->Release();
    if (FAILED(hr)) return hr;

    // Подключаем объект заднего буфера к контексту устройства
    g_pImmediateContext->OMSetRenderTargets( 1, &g_pRenderTargetView, NULL );

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

//--------------------------------------------------------------------------------------
// Удалить все созданные объекты
//--------------------------------------------------------------------------------------

void CleanupDevice()
{
    // Сначала отключим контекст устройства, потом отпустим объекты.
    if (g_pImmediateContext) g_pImmediateContext->ClearState();
    // Порядок удаления имеет значение. 
    // Удаляем эти объекты порядке, обратном тому, в котором создавали.
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
}

//--------------------------------------------------------------------------------------
// Рисование кадра
//--------------------------------------------------------------------------------------

void Render()
{
    // Просто очищаем задний буфер
    float ClearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f }; // красный, зеленый, синий, альфа-канал
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);
    // Выбросить задний буфер на экран
    g_pSwapChain->Present(0, 0);
}
