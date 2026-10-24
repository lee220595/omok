# 빈 보드 첫 수부터 대국 구현·검증

2026-10-24 사용자 후속 요청 반영. 기준 커밋 `50a3b9060ece787867cdbb1d897e985f11d99324`, 작업 브랜치 `codex/rapfi-integration`. 시작 변경0, fetch 후 원격과 일치, 기존 GUI/엔진 작업을 보존했다. main `a15a871`에는 통합하지 않는다.

## 구현 계약

- opening의 `DirectRenju`는 `begin_direct_renju(black_owner)`로 빈 보드/이력·흑 차례·A/B 색 배정·Started/ColorsAssigned/Completed 이벤트를 설정한다. ID `direct-renju-v1`. 준비 돌/BatchPlaced를 꾸며내지 않는다.
- match의 `start_direct`는 원래 Session 전체와 초기화를 보존하고 기존 place/rules·결과 적용·Token을 재사용한다. 재시작은0수/흑 차례로 돌아간다. PreparedRenju의 빈 compact 거부와 Swap2 미해결 계약은 그대로다.
- GUI 첫 목록의 초기 선택은 **처음부터 · 빈 보드 (렌주)**다. 기존 개발 배치5개는 뒤에 유지한다. 빈 보드 선택 시 compact를 지우고 비활성화하며 준비 배치 선택 시 다시 채우고 활성화한다. 흑A/백B 또는 반대 배정과 사람 대 사람/두 Rapfi 역할 조합을 사용할 수 있다.
- 빈 보드의 흑 엔진에는 BEGIN을, 나머지에는 실제 수순의 BOARD를 전송한다. own/opponent·행/열 변환·시간/오류·별도 프로세스/취소 정책은 기존 계약과 같다. [공식 BEGIN/BOARD](https://plastovicka.github.io/protocl2en.htm)를 확인했다. 이번 고정 Rapfi 소스 페이지 재조회는 cache miss였으며, 새로운 소스 실행 검증으로 보고하지 않는다.
- 렌주 보드 판정·금수패·200수·AI 한 수30초/누적180초 등 기존 정책을 유지한다. 첫 수의 위치를 강제하는 정식 오프닝 절차는 추가하지 않았다. 빈 보드 시작을 Gomocup 전문가 준비 오프닝/공식 대회 규칙 일치로 소개하지 않는다. 대회용 PreparedRenju와 분리한다.

## 자동 테스트·빌드

시작 기준86/86 통과(6.25초). 새 회귀3개를 CTest에 등록했다.

| 테스트 | 확인 내용 |
|---|---|
| opening.direct_renju | 진행 중 Swap2에서 완전 초기화, 흑 소유자A/B·0수·다음 참가자·이벤트, 잘못된 참가자 거부 시 전체 보존 |
| match.direct_renju | 준비 대국→빈 보드, 양쪽 색 배정, 차례/좌표/중복/오래된 입력 거부와 전체 보존, 실제9수 5목 승리/종료 후 차단, 재시작0수·토큰 교체·기권 |
| ai.first_move | 잘못된 actor 거부, BEGIN/BOARD와 모의 엔진 실제 프로세스 송수신, 흑 첫 수→백 차례, 사람 첫 수→엔진 백 응답, 재시작 후 이전 응답 차단·자식 정리 |

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent "$env:TEMP\omok-rules-build"
ctest --test-dir .\build\native-release --output-on-failure --no-tests=error
.\build\native-release\bin\omok_engine_probe.exe .\external\rapfi\Rapfi-engine\pbrain-rapfi-windows-avx2.exe 1000
Get-FileHash build/native-release/bin/omok.exe,dist/omok/omok.exe -Algorithm SHA256
```

첫 빌드/install 성공·CTest89/89(8.38초). GUI 확인 후 재시작 문구를 “같은 시작 방식·색 배정”으로 보완해 최종 빌드/install을 수행했다. 최종35개 컴파일/링크 성공, 경고 출력 없음, **CTest89/89 통과(8.29초)**. 상태14·규칙39·오프닝15·match15·AI6. 이후 실행 코드 변경은 없다. 빌드 스크립트와 ASCII junction 우회는 무변경이다.

최종 build/dist SHA256는 `8B725049A9888BC9307DF059E2748EE6224E7FE99F484DD9FCB51652F3AF0DF8`로 일치. 배포 README도 소스 runtime-readme와 해시가 같았다. GUI DLL은 Windows 기본 GDI32/KERNEL32/msvcrt/USER32뿐이다.

## 실제 Rapfi 실행

Rapfi0.43.01 Windows AVX2, 기존 설치 config/가중치와 CPU/자원 정책을 유지했다. 위1000ms probe는 검증용이며 GUI 제한을 완화하지 않는다. 5경우 모두 원본 상태 보존 후 match 적용·이력1개 증가, outcome0/reaped1/forced0/exit0이었다.

| probe | 입력/엔진 | 반환 | 소요ms |
|---|---|---|---:|
| 0 | 기존 준비3수·백B | I7 | 944.352 |
| 1 | 준비3수+백I9·흑A | I7 | 887.145 |
| 2 | 반대 배정 준비3수·백A | I7 | 898.837 |
| 3 | 빈 보드·흑A, BEGIN | H8 | 540.914 |
| 4 | 빈 보드에서 사람 흑B H8·백A, BOARD | G7 | 900.561 |

원시 probe 로그는 Git 제외 `build/rapfi-logs/`의 PID31520/37064 파일 등이다. 실제 GUI 로그는 `dist/omok/logs/`에 있다. 엔진 바이너리/모델/원시 로그를 Git에 포함하지 않는다.

probe3/4의 선별된 [흑 BEGIN 통신](verification/direct-start-rapfi-black.txt)·[백 BOARD 통신](verification/direct-start-rapfi-white.txt)은 재현 근거로 문서에 보존했다. 줄 끝 공백만 제거했으며 명령·응답·최종 정리 수치는 바꾸지 않았다. 갱신 문서의 로컬 링크105개 누락0, git diff --check 통과. 외부 Rapfi·build/dist Git 제외도 확인했다.

## 실제 GUI 확인과 제한

Computer Use sky로 직접 시작/클릭했다. 로컬: 빈 보드0수·흑A → H8 흑1수 → I8 백2수 → 재시작0수·흑A → 기권/백B 승리. 사람A/흑·RapfiB/백: 빈 보드 시작 → 사람H8 → AI G7 → 다시 사람 차례 → 사람 기권. RapfiA/흑·사람B/백: 빈 보드 시작 → AI H8 → 사람 I8 → AI I7 → 사람 차례를 확인했다.

GUI 실제 응답 로그(모두 outcome0/reaped1/forced0/exit0): 백G7 `engine-3-1-38240-1792830382746281700.log` 10839.1ms, 흑첫H8 `engine-4-0-41768-1792830459551493800.log` 854.223ms, 흑후속I7 `engine-4-2-25308-1792830495782464900.log` 10112.6ms. Alt+F4 후 실제 호스트 조회에서 omok/Rapfi 프로세스0개였다. 재시작 안내 한 줄 보완 전의 기능 관찰이며 최종 문자열 변경을 기능 전체 재검증으로 간주하지 않는다.

최종 install 실행 파일도 다시 열어 초기 선택/대국 시작0수·흑 선공 → 실제 H8 착수1수 → 재시작0수·흑 선공을 확인했다. “같은 시작 방식·색 배정” 문구와 격자/돌 제거를 화면에서 확인하고 Alt+F4로 닫았다.

자동9수 승리와 실제 GUI의 초반/기권 검증을 구분한다. 긴 Rapfi 완결 대국·모든 첫 수 위치/색 배정의 GUI 조합·실제 누적 예산 소진/자원 경계·최악 재귀/장시간 누수·다중 DPI·클린 Windows10/11은 미검증이다. 기존 미검증을 전부 해소했다고 보고하지 않는다. 다음은 긴 대국과 남은 GUI·오류/자원 검증이며 자체AI/Swap2/기보로 확대하지 않았다.
