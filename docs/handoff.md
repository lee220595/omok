# 오목 프로젝트 개발 인계

갱신일: 2026-10-24. **사용자 설치 Rapfi의 초기 연결과 GUI 사람 대 Rapfi를 구현·실행했다. 전체 GUI/대회/AI 단계 완료와 구분한다.**

## 기준·Git·승인

- 시작 기준: `e6cbcb6619debf9c40361f5582bd4caf85f64b33`(로컬 대국 구현/검증 기록). 기존 WIP 소스는 `5023aa2`부터 원격에 보존돼 있었다.
- 시작 미커밋 변경은 사용자 설치 external/뿐이었다. 코드 변경은 없었다. 원본 엔진/모델을 보존하고 `.gitignore`에 `/external/rapfi/` 전체 제외를 추가했다.
- fetch 후 기존 작업 브랜치/원격 일치를 확인하고 GUI 코드에 의존하므로 그 이력을 유지한 `codex/rapfi-integration`을 만들었다. main은 오프닝 단계 `a15a871`이며 이번 단계에서 통합하지 않는다. 기존 전체 GUI 수동 항목과 AI 단계 후속 항목이 남아 있기 때문이다.
- 사용자는 자체 AI 개발이 아닌 외부 Rapfi 별도 프로세스·Gomocup/Piskvork 통신·준비 렌주/응답 검증·시간/오류/이전 응답 차단·GUI 사람 대 Rapfi 연결을 승인했다. 과거 AI 구현 제외보다 이 후속 지시가 우선한다. AI 대 AI·Swap2 연결·사람 시계·기보·자체 AI로 확대하지 않았다.
- 구현·문서·재현용 텍스트 기록을 함께 커밋/일반 푸시한다. 최종 커밋 ID·푸시 결과는 완료 보고에 남긴다. 실제 상태는 git status/log와 원격으로 확인한다. 바이너리/모델/build/dist/개인 파일은 포함하지 않는다.

## 읽을 문서와 구현 상태

[AGENTS](../AGENTS.md)·[README](../README.md)·[Rapfi 설치/프로세스/변환 계약](rapfi-integration.md)·[최신 실제 검증](verification-rapfi.md)을 먼저 읽는다. [match](match-contract.md)·[state](state-model.md)·[rules](rules-spec.md)·[opening](opening-contract.md)의 기존 책임은 유지한다. 과거 [로컬 GUI 검증](verification-match.md)은 당시 미검증 기록이며 이번의 실제 클릭/AI 대국 관찰과 함께 읽는다.

- state/rules/opening/match: 기존15×15·준비 배치·렌주·200수·금수패·권한·사본 적용·세션/revision을 유지했다. match에 엔진 실패 결과 적용을 추가했다. 정상 엔진 좌표도 기존 place/rules에서 검증한다.
- ai: 실제 정적 라이브러리로 CPU 검사, 순서 보존 BOARD 변환, Win32 프로세스/제한 핸들/파이프/Job, 제한 시간, 로그, 취소 가능한 작업자를 구현했다. 자체 평가/탐색은 없다.
- gui: 로컬 사람 대 사람 외에 RapfiA/사람B·사람A/RapfiB, 자동 CPU 적합 경로/직접 경로 입력, AI 차례의 사람 입력 차단, 사람 기권, 재시작/닫기 취소, 엔진 상태/남은 예산/로그 이름을 연결했다.
- records는 INTERFACE·미구현이다. AI 대 AI·BEGIN/TURN 증분·SWAP2BOARD·Swap2 규칙 바인딩·기보/복기·사람 시계·자체 AI는 후속이다.

## 설치·선택 기준

설치 폴더는 `external/rapfi/Rapfi-engine/`이며 원본 config/가중치를 수정하지 않았다. 현재 선택은 `pbrain-rapfi-windows-avx2.exe`, ABOUT는 **Rapfi0.43.01 (clang++19.1.5, Windows SSE41 AVX2)**다.

현재 CPU Intel Core Ultra5 226V의 SSE41/AVX2/FMA/BMI2/AVXVNNI와 OS 벡터 상태를 확인했다. AVX512는 미지원이다. 자동 선택은 보수적으로 Windows AVX2/SSE만 사용한다. config는 min0.43.1·mix9svq·좌표 변환 none·스레드1·DB 비활성이다. 모델210901과 렌주 흑/백 전용 가중치를 사용한다. [전체 파일 크기/해시](verification/rapfi-installation.json)를 따른다. config 해시는 작업 전후 같았다.

공식 소스 조사 기준은 Rapfi250615/`1be1551ced57e38d53ed58f6d74bf6f8b4bdc230`이다. 로컬 바이너리와 정확한 빌드 소스 대응은 미증명이며 재배포하지 않는다. GPL/모델 조건 확인 없이 향후 제품 ZIP에 동봉하지 않는다.

## 핵심 계약과 제약

- 매 요청 새 프로세스 → ABOUT/START15 → INFO rule4·메모리/스레드/pondering → 설정 처리 확인 ABOUT → 남은 시간 → 원래 이력 순 BOARD/DONE → 좌표 → END/정리. 이전 엔진 보드를 재사용하지 않아 RESTART 불필요. TT/모델 캐시 재사용이 없어 성능 비용이 있다.
- row/column은 y/x로 정확히 변환한다. field1/2는 현재 엔진 참가자의 색 기준 own/opponent다. 준비 돌의 실제 actor 없음과 색 소유자를 혼동하지 않는다. 기본3수의 백 엔진 BOARD는 `7,7,2 / 8,7,1 / 6,8,2` 순서다.
- 형식 오류·진단·stderr·프로세스 종료를 구분하고, 16KiB 라인/256KiB 요청 기록 한도를 둔다. 범위/중복/금수는 match에서 검증한다. 초기화/지원 실패는 Aborted, 대국 중 시간 초과/잘못된 출력/종료/불법 수는 상대 승리다. 별도 재시도는 없다.
- GUI의 AI 예산은 한 수30초/누적180초, 초기화 감시10초다. 사람 시계는 없다. 초기화/매 요청 프로세스 비용도 누적 예산에서 차감한다. 모델/통신 여유를 검색 시간에서 남기며 관리자 deadline은 완화하지 않는다.
- Job은 프로세스1GiB, affinity는 한 논리CPU다. 원본 config의 TT 외 여유0 때문에 엔진에는512MiB를 전달해 모델 공간을 남긴다. INFO thread_num1/pondering0을 적용한다. 전체 대회 자원 검증 완료는 아니다.
- 작업자는 값 스냅샷만 받고 GUI를 호출하지 않는다. 취소 세대와 Token/actor/종료 상태를 작업자·UI·match에서 확인한다. 재시작/기권/닫기 후 이전 결과를 적용하지 않는다. END 후 기다린 뒤 필요한 경우 직접 생성한 엔진만 정리하고 join한다.
- GUI의 원시 로그는 실행 파일 옆 logs, probe는 build/rapfi-logs다. Git 제외이며 선별된 재현 텍스트만 docs에 보존했다. 현재 END는 별도 raw 프레임이 아닌 최종 정리 요약으로 확인한다.

## 실제 검증·명령

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent "$env:TEMP\omok-rules-build"
ctest --test-dir .\build\native-release --output-on-failure --no-tests=error
.\build\native-release\bin\omok_engine_probe.exe .\external\rapfi\Rapfi-engine\pbrain-rapfi-windows-avx2.exe 1000
Get-FileHash build/native-release/bin/omok.exe,dist/omok/omok.exe -Algorithm SHA256
```

- 최종 빌드/링크/install 성공, 컴파일 경고 출력 없음. 기존 빌드 스크립트·ASCII 경로 우회 무변경.
- **최종 CTest86/86 통과, 9.15초**. 상태14·규칙39·오프닝14·match14·AI5. AI5는 고정 응답 모의 엔진이며 실제 Rapfi 검증과 구분한다.
- 최종 C++ 실제 Rapfi 세 경우: 백B(준비3수), 흑A(준비3수+사람I9), 백A(흑B/백A 배정). 모두8,6→I7을 기존 match에 합법 적용했다. 947.307/918.763/922.211ms, 모두 reaped1/forced0/exit0. [출력](verification/rapfi-probe.txt)·[통신](verification/rapfi-case-0.txt)을 참고한다.
- GUI 실제 확인: 사람A/RapfiB 시작 → 백I7 적용 → 사람 흑H7 클릭 → 다음 AI 요청 → 재시작3수 → 기권/이전 요청 취소. 반대 역할 RapfiA/사람B에서 백I9 클릭 → 흑I7 적용 → 사람 기권도 확인했다.
- 요청 중 Alt+F4 후 실제 취소 로그와 omok/Rapfi 프로세스 소멸을 확인했다. 강제 종료는 실제 Rapfi 정상 경로에서 없었다. 모의 END 무응답 자식의 제한 정리는 별도 자동 검증했다.
- 화면 안내·격자/돌·차례·마지막 수·기권 결과와 반응성을 관찰했다. 긴 엔진 정보와 종료/재시작 문구를 보완했다. 마지막 사람 대기 문구1줄은 빌드/테스트 후 시각 재확인을 수행하지 않았다.
- build/dist 해시는 `1132A9192ECBF9DCC74599CF8E0A1A0DFC2D0BC90B2DC679ED8E27CDC7DDEB35`로 일치. GUI DLL4개는 Windows 기본 DLL뿐, Rapfi는 KERNEL32/ADVAPI32다.

## 실패·남은 조건·다음 작업

실제1GiB TT 할당 실패와 지연 가중치 로드에 따른1초 시험 시간 초과를 확인해512MiB 예산·설정 확인/시간 차감·검색 여유로 수정했다. 첫 CMake Threads 가져오기 범위 오류도 수정했다. 실패 기록을 성공으로 덮지 않고 [검증 기록](verification-rapfi.md)에 구분했다.

이번 목표인 외부 엔진 초기화/준비 한 수·match 적용·GUI 사람 대 Rapfi 초기 연결은 수행했다. **전체5/6단계·대회 적합성은 미완료다.** 실제 승리/금수/200수까지 긴 Rapfi 대국, 누적180초 소진·실제 오류 주입, 자원 피크/경계·장시간 누수·다중 모니터 DPI, 개발 도구 없는 Windows10/11·MSVC는 미검증이다. 기존 로컬 GUI의 미검증 항목도 자동으로 완료하지 않는다.

다음 작업은 현재 브랜치/원격·해시를 확인하고 실제 장기 대국/각 종료·오류·마지막 표시 변경을 검증하는 것이다. 새 프로세스 방식의 비용을 측정한 뒤 같은 규칙/순서/Token 계약을 유지하는 프로세스 재사용을 검토할 수 있다. AI 대 AI·Swap2 통신/규칙·공식 데이터·외부 매니저 전체 검증은 별도 범위로 진행한다. 자체AI를 시작하지 않는다. main 통합은 남은 단계 검증과 기존 이력 보존 정책에 따라 판단한다.
