# Rapfi 외부 프로세스 연결 계약

2026-10-24. 사용자 설치 Rapfi를 기존 match에 연결한다. 자체 평가/탐색/학습 알고리즘은 개발하지 않는다. 기준 코드는 로컬 GUI 단계 `e6cbcb6619debf9c40361f5582bd4caf85f64b33`이며 `codex/rapfi-integration`이 이를 이어받는다. 미완료 GUI 단계를 없던 일로 만들거나 main을 재설정하지 않는다.

## 설치·CPU·출처

실제 설치는 `external/rapfi/Rapfi-engine/`이다. Windows exe 다섯 개·Linux 다섯 개·macOS 하나·config.toml·고전 모델·NNUE 네 가중치·AUTHORS가 있다. 자동 선택은 CPUID와 OSXSAVE/XGETBV로 OS의 벡터 상태 지원까지 확인한다. SSE2/SSSE3/SSE4.1이 필요하며 AVX2/FMA/BMI2가 모두 있으면 Windows AVX2를 선택한다. AVX512/VNNI는 자동 선택하지 않는다. 현재 Intel Core Ultra 5 226V는 AVX2/FMA/BMI2·AVXVNNI 지원, AVX512 미지원이므로 AVX2를 사용했다.

필수 배치:

```text
external/rapfi/Rapfi-engine/
  pbrain-rapfi-windows-avx2.exe   # 현재 PC. 구형 CPU는 windows-sse.exe
  config.toml
  model210901.bin
  mix9svqrenju_bs15_black.bin.lz4
  mix9svqrenju_bs15_white.bin.lz4
  ...                           # 제공받은 나머지 파일도 보존
```

현재 config는 최소 버전 0.43.1, evaluator mix9svq, 좌표 변환 none, 기본1스레드, DB 기본 비활성이다. 기본 TT 32768KiB, memory_reserved_mb=0이다. 파일을 수정하지 않는다. GUI 사전 검사는 exe·위 네 설정/모델 파일 존재와 coord_conversion_mode="none"을 확인하며 TOML 전체를 해석/대체하지 않는다. 잘못된 설정은 기존 대국을 보존한 채 시작 거부한다. 엔진 자체의 config/가중치 오류도 로그와 함께 시작 실패로 구분한다.

실제 ABOUT는 `Rapfi 0.43.01 (clang++ 19.1.5 on Windows SSE41 AVX2)`였다. [로컬 파일 전체 해시](verification/rapfi-installation.json)를 실행 재현 기준으로 삼는다. 제작자 저장소는 dhbloo에서 [dotb1ue/rapfi](https://github.com/dotb1ue/rapfi)로 연결된다. 조사 소스는 [250615/1be1551](https://github.com/dotb1ue/rapfi/tree/1be1551ced57e38d53ed58f6d74bf6f8b4bdc230)이지만 사용자 다운로드 ZIP/정확한 빌드 소스와의 바이너리 동일성은 증명하지 않았다. 소스는 조사용 build 캐시에만 두고 제품에 복사/번역하지 않았다.

`external/rapfi/` 전체를 Git·install에서 제외한다. Rapfi는 GPLv3 안내가 있으며 바이너리/모델 재배포 조건을 일괄 승인된 것으로 해석하지 않는다. 이번에는 사용자 로컬 설치를 실행하고 원본 config/가중치를 보존한다.

## 변환·명령과 응답

공식 [Gomoku AI protocol](https://plastovicka.github.io/protocl2en.htm)과 고정 [Rapfi gomocup.cpp](https://github.com/dotb1ue/rapfi/blob/1be1551ced57e38d53ed58f6d74bf6f8b4bdc230/Rapfi/command/gomocup.cpp)를 확인했다. 전체 대회 호환성이나 모든 확장 명령 지원을 뜻하지 않는다.

상태 좌표 {row,column}은 x=column, y=row이며 둘 다 0..14다. 표시 I7은 상태{6,8}, 프로토콜8,6이다. BOARD는 현재 엔진 참가자의 색을 기준으로 자신의 돌1/상대 돌2를 부여하며 A/B나 흑/백 enum을 숫자로 캐스팅하지 않는다. 준비 돌의 placed_by=nullopt는 색 판별에 사용하지 않는다. 반드시 game.history() 원래 수순대로 전송한다. 색 선택/배정 때문에 기존 돌의 순서를 정렬하지 않는다.

예: 준비3수 h8i8g9, 엔진이 백이면 다음과 같다.

```text
BOARD
7,7,2
8,7,1
6,8,2
DONE
```

첫 통합은 **매 착수 요청마다 새 프로세스**를 실행한다. ABOUT → START15/OK → INFO rule4·메모리/1스레드/pondering0·ABOUT 설정 처리 확인 → 시간INFO → 전체BOARD/DONE → 좌표 → END → 종료 대기다. 새 프로세스라 이전 보드가 없으며 RESTART가 필요 없다. 중간 상태 누적 오류를 피하고 전체 배치를 정확하게 복원하지만 TT/가중치 캐시를 재사용하지 않아 시작 비용과 실력/성능 영향이 있다. 이를 영구 설계나 최적 대회 운영으로 확정하지 않는다. BEGIN/TURN 증분 송신·SWAP2BOARD·AI 대 AI는 이 구현의 후속이다. 기본 준비 렌주를 Swap2와 결합하지 않는다.

CR/LF/CRLF·분할된 라인·빈 줄을 처리한다. MESSAGE/DEBUG만 진단으로 수용하며 stderr는 별도로 배수해 로그에 넣는다. 좌표는 정수 두 개/쉼표/줄 전체 형식으로 읽고 범위·점유·권한·금수는 match에서 확인한다. ERROR/UNKNOWN은 시작/지원 실패, 예상 밖 출력은 오류다. 16KiB 초과 라인·256KiB 초과 요청 로그는 출력 오류로 중단해 무제한 버퍼를 방지한다.

## 시간·자원·결과

GUI는 AI 한 수30초·누적180초를 사용한다. 초기화는 기술적 감시 한도10초이며 초기화 시간도 누적 예산에서 차감한다. 한 수는 START 완료 뒤 설정 처리/가중치/응답까지 계측한다. 누적 예산은 프로세스 시작부터 종료 정리까지 보수적으로 차감한다. 사람 사고 시간은 AI 예산에서 빼지 않으며 사람 시계는 미구현이다. 전체 대회 시계 검증과 다르다. CLI probe의1000ms는 검증용이며 연습용 UI 시간 설정을 추가한 것이 아니다.

Rapfi가 NNUE를 BOARD에서 지연 로드하고 자신의 검색 시계에는 이 비용을 포함하지 않는 점을 실제로 확인했다. 관리자 deadline을 늘리지 않고 검색에 남은 시간에서 최대1초(짧은 요청은 절반) 여유를 남겨 전달한다. INFO/설정 처리 확인에 쓴 시간도 차감한다. 해당 여유가 모든 장치의 로드 시간 상한임을 보증하지 않는다. 초과 시 기존 시간 초과 정책을 적용한다.

1GiB 프로세스 Job 제한, 한 논리 CPU affinity, INFO thread_num1/pondering0을 적용한다. config가 모델 외 여유를0으로 두므로 INFO max_memory는512MiB로 전달해 TT 외 공간을 확보한다. 최초1GiB 전달에서 실제 TT 할당 실패를 확인한 뒤 보완한 것으로, 1GiB 제한을 완화하지 않았다. 전체 엔진 자원 정책 준수/피크 사용량 정밀 검증은 후속이다.

정상 좌표는 apply_result → Controller::place → 기존 rules/상태 사본 적용을 거친다. 구조 불법 좌표/중복은 엔진 IllegalMove로 상대 승리, 흑 금수는 기존 금수패다. 시작/초기화/지원 실패는 Aborted(승패 없음), 응답 시간 초과·대국 중 종료·잘못된 출력은 상대 승리로 종료한다. 응답 직후 비정상 종료도 확인한다. 재시도하지 않는다. 엔진의 선택이 불법인 것과 별도 심판이 다른 승패를 판정하는 충돌을 혼동하지 않는다. 외부 매니저 판정 충돌 정책은 그대로 유지한다.

## 프로세스·비동기 수명

CreateProcessW에 명시적 exe/인용된 argv0·exe 폴더 cwd·숨김 창·제한된 세 개 표준 핸들만 전달한다. shell을 거치지 않는다. 중단 가능한 작업자는 GameState/Token/actor 사본을 소유하고 HWND나 가변 Controller를 참조하지 않는다. UI 타이머가 완료 결과를 가져온다.

EngineWorker는 대기 요청 하나를 최신 요청으로 합치고 cancel/restart/기권/닫기 시 세대를 바꾼다. 원래 Token과 현재 Token이 다르거나 종료 상태이면 결과를 폐기한다. match도 같은 Token/actor를 재검사한다. END 후 최대1초 대기하고 필요하면 **직접 생성한 프로세스만** 종료하며, kill-on-close Job이 자손/핸들 누수를 막는다. 무관한 프로세스는 검색 종료하지 않는다. 작업자 소멸은 cancel/notify/join이며 UI 호출은 없다. 극단적인 OS 호출 지연·자손 생성/장시간 누수는 미검증이다.

로그는 GUI 실행 파일 옆 logs, probe는 build/rapfi-logs에 기록한다. 이름에 세션/revision/PID/시각을 넣으며 송수신·결과·종료코드·정리 여부를 보존한다. 로그와 외부 모델은 Git 제외다. 재현용으로 선별한 실제 통신 텍스트만 docs/verification에 보관한다.
