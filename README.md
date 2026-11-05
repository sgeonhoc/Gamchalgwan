# 감찰관 — 부두 그레이박스 (LAUNCH:ER 3기 2주차 프로토타입)

밤 부두 3번 창고에서 반출 기록을 손에 넣고 빠져나가는 한 판짜리 그레이박스다.
관리인에게 기록을 요청하는 순간부터 관리인이 누군가에게 보고하기 시작하고, 시간을 쓰는 행동을 할 때마다 그 보고가 쌓인다.
기록을 받으면 정문으로 바로 나갈 수도 있고, 기록을 근거로 조합에 연락해 후문을 열게 할 수도 있다. 끝은 넷이다.

## 2주차 제출물 (v0.1.0)

| 항목 | 링크 |
|---|---|
| 설치본 (zip · 308.6 MB) | [Gamchalgwan_v0.1.0_20261105-1904.zip](https://github.com/sgeonhoc/Gamchalgwan/releases/download/v0.1.0/Gamchalgwan_v0.1.0_20261105-1904.zip) · [릴리스 페이지](https://github.com/sgeonhoc/Gamchalgwan/releases/tag/v0.1.0) |
| 설치 테스트 결과 | [docs/install_test_v0.1.0.md](docs/install_test_v0.1.0.md) |
| 빌드 스크립트 | [tools/build_unreal.bat](tools/build_unreal.bat) · 설치본에 넣는 안내문 [tools/README_PLAY.txt](tools/README_PLAY.txt) |
| 체인지로그 | [CHANGELOG.md](CHANGELOG.md) |
| 플레이 클립 | [12초](https://github.com/sgeonhoc/Gamchalgwan/releases/download/v0.1.0/Gamchalgwan_v0.1.0_clip_12s.mp4) · [두 판 전체 63초](https://github.com/sgeonhoc/Gamchalgwan/releases/download/v0.1.0/Gamchalgwan_v0.1.0_clip.mp4) |

## 해 보기 (설치본)

1. [릴리스 페이지](https://github.com/sgeonhoc/Gamchalgwan/releases/tag/v0.1.0)에서 `Gamchalgwan_v0.1.0_*.zip` 을 받아 아무 폴더에나 푼다.
2. `Windows\Gamchalgwan.exe` 를 실행한다. 파란 「Windows의 PC 보호」 창이 뜨면 「추가 정보」 → 「실행」.
3. 조작: 마우스 왼쪽 클릭(대상 고르기 · 버튼) · `R` 처음부터 · `Esc` 끝내기 · `F1` 개발 정보.

자세한 안내는 설치본 안의 `README_PLAY.txt` 에 있다.

## 빌드하기

- 필요한 것: Unreal Engine 5.8 (Epic 런처 설치본) · Visual Studio 2022 C++ 도구.
- `tools\build_unreal.bat 0.1.0` 을 실행하면 `C:\LE3\Builds\Gamchalgwan_v0.1.0_<날짜>.zip` 과 같은 이름의 `.log` 가 나온다.
  - 엔진 · 출력 폴더가 다르면 bat 맨 위의 `UE_ROOT` · `OUT_ROOT` 를 고치거나 같은 이름의 환경 변수로 넘긴다.
  - 다른 일과 같이 돌릴 때는 `MAX_JOBS`(동시 컴파일 수, 기본 4)를 낮춘다.
- 실패하면 `.log` 에서 처음 나오는 `Error` 줄부터 본다.

## 짜임

- 에셋을 쓰지 않는다. 엔진의 `/Engine/Maps/Entry` 맵 위에 `ADockGameMode` 가 `/Engine/BasicShapes` 로 판 전체를 깐다.
- `Source/Gamchalgwan/` — `DockGameMode`(규칙 · 대역 여섯 · 결말) · `DockHUD`(화면 글과 버튼, Canvas) · `DockPlayerController`(클릭 · 키).
- 다이얼은 `Config/DefaultGame.ini` 의 `[/Script/Gamchalgwan.DockTuning]` 에 있다. 값을 고친 뒤 에디터 `-game` 으로 바로 돌려 보거나, bat 로 설치본을 다시 뽑는다(C++ 는 다시 짤 필요가 없다).
- 한글 글꼴(Droid Sans Fallback, Apache 2.0)과 Roboto(Apache 2.0)는 `Content/Fonts` 에 그대로 두고 설치본에 낱파일로 넣는다.

## 플레이 클립 다시 뽑기

```
Gamchalgwan.exe -autoplay -capdir=C:\cap -RenderOffscreen -ResX=1280 -ResY=720
ffmpeg -framerate 30 -i C:\cap\f_%05d.png -i C:\cap\audio.wav -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest clip.mp4
```

자동 플레이는 고정 1/30초로 두 판(붙잡힘 → 후문)을 돌고, 프레임 · 소리 · `done.txt` 를 남긴 뒤 스스로 꺼진다.

## 기록

- 바뀐 것: [CHANGELOG.md](CHANGELOG.md)
