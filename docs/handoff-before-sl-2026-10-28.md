# 오목 프로젝트 개발 인계

갱신: 2026-10-28. 승인 범위는 **기존 C++ 기반의 Python 무화면 환경·순차 자동 대국·기록/재생·재개·계측 구현**이다. 모델/지도학습/추론/MCTS/RL/제출 수신기/GUI 개선은 이번 범위에 없고 시작하지 않았다. 이전 인계는 [보존본](handoff-before-python-2026-10-28.md)에 있다.

## 현재 코드와 변경

- src/headless/bridge.cpp와 CMake: 선택적 omok_headless DLL, ABI1, Controller 생성/복사/정리·direct/prepared 초기화·스냅샷/마스크/착수/닫기. 기존 state/rules/opening/match 판정과 금수패·200수 우선순위를 재사용했다. 기존 코어/GUI/외부 엔진 소스는 변경하지 않았다.
- python/omok_env: ctypes 환경·0..224행동·4채널 float32 관측·고유 game ID/Token·독립 복제, JSON trace 안전 저장/별도 환경 재생, ordered/random·시드·참가자 흑백 교대·완료 단위 재개·오류/중단 분리, benchmark. Python 판정 복제·모델·보상 업데이트 없음.
- python/test_headless.py: 소형 자체 fixture와 10개 테스트. CMake 옵션 ON일 때 Python 발견 시 headless.python CTest 등록. 기본 옵션 OFF는 기존 빌드를 유지한다.
- scripts/build.ps1에 -Headless 추가. .gitignore에 runs/Python 캐시 제외. README·계약·계획·기술 결정·배포 고지와 [실제 명령/API/기록/측정](python-headless.md) 갱신.

## 실제 검증과 실행

검증 OS Windows11 x64 10.0.26200, Python3.13.7 x64, GCC13.3.0 MinGW-w64 POSIX SEH, CMake3.29.3, Ninja1.12.1. 새 의존성 설치 없음. 기본 빌드/install 성공 후 선택 DLL 빌드/install도 성공했다. ASCII 별칭은 C:/Users/leesn/AppData/Local/Temp/omok-headless-build/omok-build-71f60837f90e다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent C:/Users/leesn/AppData/Local/Temp/omok-headless-build -Headless
$env:PYTHONPATH='python'
python -m unittest discover -s python -p test_headless.py -v
ctest --test-dir build/native-release --output-on-failure --no-tests=error
python -m omok_env.runner runs/smoke --games 2 --seed 17
python -c "from omok_env.records import replay; print(replay('runs/smoke/game-000000.json')['result'])"
python -m omok_env.benchmark runs/benchmark-final --samples 30 --games 4
```

- 기존 CTest89개 통과(11.59초). 선택 DLL/Python 등록 후 최종 전체90개 통과(14.02초, Python 내부10개 검사). Python 별도10개도 통과(3.772초).
- 좌표225왕복, 4개 환경의 마스크/실제 판정900개 비교, 정확한5목 우선·금수 시도 반칙패/보드 불변·200수 무승부·종료 후 입력, 복원/관점·복제 독립·잘못된 요청/오래된 Token 보존·100회 생성/정리 확인. 기존 rules 회귀가 재귀 예외·대칭·200번째 승리/금수 우선을 검증한다.
- Python 시작→완료 두 판(seed17), 총19수/2승리·독립 재생 일치. 테스트에서 동일 시드 두 실행의 JSON 일치, 완료 재개0수/중복 집계 없음, 설정 변경 재개 거부, cap·Ctrl+C·주입 실행 오류/시작 실패 분리와 손상 파일 거부 확인. native 커밋 직후 취소를 주입해 마지막 수 보존도 확인.
- 최종 코드 소표본 처리량: Intel Core Ultra5 226V, 논리CPU8, 단일 프로세스 순차/affinity 고정 없음. 30회 평균 ms: 초기화0.00848, 상태0.01421, 관측0.05751, 준비3수 mask0.06353/위협8수0.16688, step0.02711, clone+dispose0.00556. 무작위4판/531수/4승리 저장·재생 포함1.01438초(523.47수/초·3.943판/초). 원본 결과 runs/benchmark-final/benchmark.json은 생성물로 Git 제외. 소수 표본이며 장시간/밀집 최악 성능 보증 아님.
- 초기 sandbox 임시 디렉터리에서 Python tempfile 생성이 반복 대기해 중단하고 테스트 임시 경로를 writable build/python-tests로 수정했다. 기존 충돌 alias는 삭제하지 않고 새 부모 경로로 빌드했다. Ninja의 한글 의존성 캐시는 기존 build.ps1 방식으로 재생성했다. 최종 검사에는 이 문제 없음.

## 미검증·제약·다음 작업 하나

Linux/Colab/MSVC/다른 Python·클린 Windows·장시간/대량/병렬·dense 최악 재귀·실제 심판 새 실행·실제 Rapfi/GUI 수동 실행은 검증하지 않았다. 과거 GUI/외부 엔진 미검증은 기존 verification 문서를 보존한다. 현재 프로필은 기존 고정200수이며 대회 시계/자원 프로필 분리는 아직 없다. 공식 준비 데이터/권리는 미확보, 자체 fixture는 공식 데이터가 아니다. 합법 수0은 blocked/aborted로 격리하고 draw/패스를 추가하지 않는다.

재개는 동일 config/commit/소스·DLL해시에서 완료 대국을 검증 후 건너뛰며 미완료는 attempt 보존 후 처음부터 재실행한다. 동시 output 공유는 미지원이다. 강제 종료/전원 손실에서 진행 중 대국은 유실될 수 있다. 정상 완료 파일은 남으며 임시/손상 파일을 정상 집계하지 않는다. 샘플 정제/권리·라벨·분할은 아직 P2 후속이다. trace 저장은 강함/학습 완료/대회 적합성 증명이 아니다.

**다음 작업 하나: 이 환경 위에 작은 정책·가치 모델의 지도학습과 추론 연결.** 데이터 권리·라벨/분할·중복과 모델 계약/도구 선택을 먼저 확인한다. 이번 요청으로 자동 착수하지 않는다.

## Git·기준과 승인/중단 지점

시작 d7b3142, codex/learned-policy-foundation, 추적/미추적 변경 없음. fetch 성공 후 origin/main보다 기존 이력10커밋 앞/뒤0을 확인했다. main으로 되돌려 미통합 구현을 잃지 않도록 이 이력에서 codex/python-headless-games를 만들었다. 기존 브랜치·이력·원격은 보존하며 main 통합/강제 푸시/브랜치 삭제 없음.

관련 소스·작은 테스트·문서만 커밋하고 같은 브랜치에 일반 푸시한다. build/dist/runs/외부 바이너리·가중치/임시물은 제외한다. 최종 커밋 ID와 실제 푸시 여부는 최종 보고에 기록하며 이를 문서에 넣기 위한 반복 커밋은 하지 않는다. 문서 상대 링크309개 누락0·git diff --check 통과를 확인했다. 현재 승인 범위의 완료/검증/문서·Git 저장 후 중단하며 다음 모델 작업은 착수하지 않는다.
