# Projekt Grafika – Symulacja Linii Produkcyjnej z Robotami IRB1200

Symulacja 3D hali przemysłowej z dwoma robotami przemysłowymi (wzorowanymi na ABB IRB1200) wykonującymi spawanie i lakierowanie produktów przesuwanych taśmociągiem. Napisana w C z użyciem OpenGL/Win32.

## Funkcje

- Dwa stanowiska robotyczne z animowaną kinematiką ramion (6 osi)
- Taśmociąg transportujący trzy typy produktów (kątowniki, lokomotywa, samochód)
- Efekty cząsteczkowe: iskry spawalnicze i spray lakierniczy
- Teksturowana scena: podłoga, ściany, sufit, regały, brama wjazdowa, wentylacja
- System oświetlenia z lampami sufitowymi i trybem nocnym
- Swobodna kamera FPS (WASD + mysz) oraz widoki predefiniowane
- Panel HUD z kontrolą prędkości symulacji, pauzą i ręczną konfiguracją kątów robotów
- Licznik ukończonych produktów

## Wymagania

- Windows (aplikacja Win32)
- Visual Studio 2022
- OpenGL 1.x / GLU (dostarczane przez system)
- NuGet: `nupengl.core.redist` (GLFW, GLEW, freeglut – dołączone w folderze `packages/`)

## Uruchomienie

1. Otwórz `projektgrafika.sln` w Visual Studio 2022.
2. Paczki NuGet są już w folderze `packages/` – nie trzeba pobierać.
3. Zbuduj projekt (Ctrl+Shift+B) i uruchom (F5).

## Sterowanie

| Klawisz / akcja | Opis |
|---|---|
| `F` | Przełącz tryb swobodnej kamery FPS |
| `W A S D` | Ruch kamery (tryb FPS) |
| PPM + ruch myszy | Obrót kamery (tryb FPS) |
| `1` / `2` / `3` | Widoki predefiniowane |
| `Space` | Pauza / wznowienie symulacji |
| `N` | Tryb nocny |
| Suwak HUD | Prędkość symulacji (0.1× – 3.0×) |
| Przycisk `Config` | Ręczne sterowanie kątami robotów |

## Struktura projektu

```
projektgrafika/
├── projekt grafika.c     # Cały kod źródłowy
├── projektgrafika.vcxproj
├── projektgrafika.filters
├── packages.config
├── *.bmp                 # Tekstury (podłoga, metal, drewno, taśma, …)
packages/
└── nupengl.core.redist.0.1.0.1/   # GLFW, GLEW, freeglut (x64 + Win32)
```

## Autorzy

Projekt zaliczeniowy z grafiki komputerowej.
