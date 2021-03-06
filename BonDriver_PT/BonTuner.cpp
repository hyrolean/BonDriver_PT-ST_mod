#include "stdafx.h"
#include <Windows.h>
#include <process.h>

#include "inc/EARTH_PT.h"
#include "inc/OS_Library.h"
using namespace EARTH;

#include "BonTuner.h"

static BOOL isISDB_S;

#define DATA_BUFF_SIZE 188*256
#define MAX_BUFF_COUNT 500

#pragma warning( disable : 4273 )
extern "C" __declspec(dllexport) IBonDriver * CreateBonDriver()
{
	// 同一プロセスからの複数インスタンス取得禁止
	// (非同期で取得された場合の排他処理がちゃんと出来ていないが放置)
	CBonTuner *p = NULL;
	if (CBonTuner::m_pThis == NULL) {
		p = new CBonTuner;
	}
	return p;
}
#pragma warning( default : 4273 )

CBonTuner * CBonTuner::m_pThis = NULL;
HINSTANCE CBonTuner::m_hModule = NULL;


CBonTuner::CBonTuner()
{
	m_pThis = this;
	m_hOnStreamEvent = NULL;

	m_LastBuff = NULL;

	m_dwCurSpace = 0xFF;
	m_dwCurChannel = 0xFF;
	m_hasStream = TRUE;

	m_iID = -1;
	m_hStopEvent = _CreateEvent(FALSE, FALSE, NULL);
	m_hThread = NULL;

	::InitializeCriticalSection(&m_CriticalSection);

	WCHAR strExePath[512] = L"";
	GetModuleFileName(m_hModule, strExePath, 512);

	WCHAR szPath[_MAX_PATH];	// パス
	WCHAR szDrive[_MAX_DRIVE];
	WCHAR szDir[_MAX_DIR];
	WCHAR szFname[_MAX_FNAME];
	WCHAR szExt[_MAX_EXT];
	_tsplitpath_s( strExePath, szDrive, _MAX_DRIVE, szDir, _MAX_DIR, szFname, _MAX_FNAME, szExt, _MAX_EXT );
	_tmakepath_s(  szPath, _MAX_PATH, szDrive, szDir, NULL, NULL );

	m_strPT1CtrlExe = szPath;
	m_strPT1CtrlExe += L"PTCtrl.exe";

	wstring strIni;
	strIni = szPath;
	strIni += L"BonDriver_PT-ST.ini";

	int iPTn = GetPrivateProfileIntW(L"SET", L"PTver", 2, strIni.c_str());
	if ((iPTn != 2) && (iPTn != 1))
		iPTn = 2;

	isISDB_S = TRUE;

	WCHAR szName[256];
	m_iTunerID = -1;
	if( wcslen(szFname) == wcslen(L"BonDriver_PT-**") ){
		const WCHAR *TUNER_NAME2;
		if (szFname[13] == L'T'){
			isISDB_S = FALSE;
			TUNER_NAME2 = L"PT%d ISDB-T (%d)";
		}else{
			TUNER_NAME2 = L"PT%d ISDB-S (%d)";
		}
		m_iTunerID = _wtoi(szFname+wcslen(L"BonDriver_PT-*"));
		wsprintfW(szName, TUNER_NAME2, iPTn, m_iTunerID);
		m_strTunerName = szName;
	}else if( wcslen(szFname) == wcslen(L"BonDriver_PT-*") ){
		const WCHAR *TUNER_NAME;
		if (szFname[13] == L'T'){
			isISDB_S = FALSE;
			TUNER_NAME = L"PT%d ISDB-T";
		}else{
			TUNER_NAME = L"PT%d ISDB-S";
		}
		wsprintfW(szName, TUNER_NAME, iPTn);
		m_strTunerName = szName;
	}else{
		wsprintfW(szName, L"PT%d ISDB-S", iPTn);
		m_strTunerName = szName;
	}

	wstring strChSet;

	//dll名と同じ名前の.ChSet.txtを先に優先して読み込みを試行する
	//(fixed by 2020 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
	strChSet = szPath;	strChSet += szFname;	strChSet += L".ChSet.txt";
	if(!m_chSet.ParseText(strChSet.c_str())) {
		strChSet = szPath;
		if (isISDB_S)
			strChSet += L"BonDriver_PT-S.ChSet.txt";
		else
			strChSet += L"BonDriver_PT-T.ChSet.txt";
		if(!m_chSet.ParseText(strChSet.c_str()))
			BuildDefSpace(strIni);
	}
}

CBonTuner::~CBonTuner()
{
	CloseTuner();

	::EnterCriticalSection(&m_CriticalSection);
	SAFE_DELETE(m_LastBuff);
	::LeaveCriticalSection(&m_CriticalSection);

	::CloseHandle(m_hStopEvent);
	m_hStopEvent = NULL;

	::DeleteCriticalSection(&m_CriticalSection);

	m_pThis = NULL;
}

void CBonTuner::BuildDefSpace(wstring strIni)
{
	//.ChSet.txtが存在しない場合は、既定のチャンネル情報を構築する
	//(added by 2021 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

	BOOL UHF=TRUE, CATV=FALSE, VHF=FALSE, BS=TRUE, CS110=TRUE;
	DWORD BSStreams=8, CS110Streams=8;
	BOOL BSStreamStride=FALSE, CS110StreamStride=FALSE;

#define LOADDW(nam) do {\
		nam=(DWORD)GetPrivateProfileIntW(L"DefSpace", L#nam, nam, strIni.c_str()); \
	}while(0)

	LOADDW(UHF);
	LOADDW(CATV);
	LOADDW(VHF);
	LOADDW(BS);
	LOADDW(CS110);
	LOADDW(BSStreams);
	LOADDW(CS110Streams);
	LOADDW(BSStreamStride);
	LOADDW(CS110StreamStride);

#undef LOADDW

	DWORD spc=0 ;
	auto entry_spc = [&](const wchar_t *space_name) {
		SPACE_DATA item;
		item.wszName=space_name;
		item.dwSpace=spc++;
		m_chSet.spaceMap.insert( pair<DWORD, SPACE_DATA>(item.dwSpace,item) );
	};

	if(isISDB_S) {  // BS / CS110

		DWORD i,ch,ts,pt1offs;
		auto entry_ch = [&](const wchar_t *prefix, bool suffix) {
			CH_DATA item ;
			Format(item.wszName,suffix?L"%s%02d/TS%d":L"%s%02d",prefix,ch,ts);
			item.dwSpace=spc;
			item.dwCh=i;
			item.dwPT1Ch=(ch-1)/2+pt1offs;
			item.dwTSID=ts;
			DWORD iKey = (item.dwSpace<<16) | item.dwCh;
			m_chSet.chMap.insert( pair<DWORD, CH_DATA>(iKey,item) );
		};

		if(BS) {
			pt1offs=0;
			if(BSStreamStride) {
				for(i=0,ts=0;ts<(BSStreams>0?BSStreams:1);ts++)
				for(ch=1;ch<=23;ch+=2,i++)
					entry_ch(L"BS",BSStreams>0);
			}else {
				for(i=0,ch=1;ch<=23;ch+=2)
				for(ts=0;ts<(BSStreams>0?BSStreams:1);ts++,i++)
					entry_ch(L"BS",BSStreams>0);
			}
			entry_spc(L"BS");
		}

		if(CS110) {
			pt1offs=12;
			if(CS110StreamStride) {
				for(i=0,ts=0;ts<(CS110Streams>0?CS110Streams:1);ts++)
				for(ch=2;ch<=24;ch+=2,i++)
					entry_ch(L"ND",CS110Streams>0);
			}else {
				for(i=0,ch=2;ch<=24;ch+=2)
				for(ts=0;ts<(CS110Streams>0?CS110Streams:1);ts++,i++)
					entry_ch(L"ND",CS110Streams>0);
			}
			entry_spc(L"CS110");
		}

	}else { // 地デジ

		DWORD i,offs,C;
		auto entry_ch = [&](int (*pt1conv)(int i)) {
			CH_DATA item;
			Format(item.wszName,C?L"C%dCh":L"%dCh",i+offs);
			item.dwSpace=spc;
			item.dwCh=i;
			item.dwPT1Ch=pt1conv(i);
			DWORD iKey = (item.dwSpace<<16) | item.dwCh;
			m_chSet.chMap.insert( pair<DWORD, CH_DATA>(iKey,item) );
		};

		if(UHF) {
			for(offs=13,C=i=0;i<50;i++) entry_ch([](int i){return i+63;});
			entry_spc(L"地デジ(UHF)") ;
		}

		if(CATV) {
			for(offs=13,C=1,i=0;i<51;i++) entry_ch([](int i){return i+(i>=10?12:3);});
			entry_spc(L"地デジ(CATV)") ;
		}

		if(VHF) {
			for(offs=1,C=i=0;i<12;i++) entry_ch([](int i){return i+(i>=3?10:0);});
			entry_spc(L"地デジ(VHF)") ;
		}

	}
}

const BOOL CBonTuner::OpenTuner(void)
{
	//イベント
	m_hOnStreamEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	ZeroMemory(&si,sizeof(si));
	si.cb=sizeof(si);

	BOOL bRet = CreateProcessW( NULL, (LPWSTR)m_strPT1CtrlExe.c_str(), NULL, NULL, FALSE, GetPriorityClass(GetCurrentProcess()), NULL, NULL, &si, &pi );
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	DWORD dwRet;
	if( m_iTunerID >= 0 ){
		dwRet = SendOpenTuner2(isISDB_S, m_iTunerID, &m_iID);
	}else{
		dwRet = SendOpenTuner(isISDB_S, &m_iID);
	}

	_RPT3(_CRT_WARN, "*** CBonTuner::OpenTuner() ***\nm_hOnStreamEvent[%p] bRet[%s] dwRet[%u]\n", m_hOnStreamEvent, bRet ? "TRUE" : "FALSE", dwRet);

	if( dwRet != CMD_SUCCESS ){
		return FALSE;
	}

	m_hThread = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (LPVOID)this, CREATE_SUSPENDED, NULL);
	ResumeThread(m_hThread);

	return TRUE;
}

void CBonTuner::CloseTuner(void)
{
	if( m_hThread != NULL ){
		::SetEvent(m_hStopEvent);
		// スレッド終了待ち
		if ( ::WaitForSingleObject(m_hThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(m_hThread, 0xffffffff);
		}
		CloseHandle(m_hThread);
		m_hThread = NULL;
	}

	m_dwCurSpace = 0xFF;
	m_dwCurChannel = 0xFF;
	m_hasStream = TRUE;

	::CloseHandle(m_hOnStreamEvent);
	m_hOnStreamEvent = NULL;

	if( m_iID != -1 ){
		SendCloseTuner(m_iID);
		m_iID = -1;
	}

	//バッファ解放
	::EnterCriticalSection(&m_CriticalSection);
	while (!m_TsBuff.empty()){
		TS_DATA *p = m_TsBuff.front();
		m_TsBuff.pop_front();
		delete p;
	}
	::LeaveCriticalSection(&m_CriticalSection);
}

const BOOL CBonTuner::SetChannel(const BYTE bCh)
{
	return TRUE;
}

const float CBonTuner::GetSignalLevel(void)
{
	if( m_iID == -1 || !m_hasStream){
		return 0;
	}
	DWORD dwCn100;
	if( SendGetSignal(m_iID, &dwCn100) == CMD_SUCCESS ){
		return ((float)dwCn100) / 100.0f;
	}else{
		return 0;
	}
}

const DWORD CBonTuner::WaitTsStream(const DWORD dwTimeOut)
{
	if( m_hOnStreamEvent == NULL ){
		return WAIT_ABANDONED;
	}
	// イベントがシグナル状態になるのを待つ
	const DWORD dwRet = ::WaitForSingleObject(m_hOnStreamEvent, (dwTimeOut)? dwTimeOut : INFINITE);

	switch(dwRet){
		case WAIT_ABANDONED :
			// チューナが閉じられた
			return WAIT_ABANDONED;

		case WAIT_OBJECT_0 :
		case WAIT_TIMEOUT :
			// ストリーム取得可能
			return dwRet;

		case WAIT_FAILED :
		default:
			// 例外
			return WAIT_FAILED;
	}
}

const DWORD CBonTuner::GetReadyCount(void)
{
	DWORD dwCount = 0;
	::EnterCriticalSection(&m_CriticalSection);
	if(m_hasStream) dwCount = (DWORD)m_TsBuff.size();
	::LeaveCriticalSection(&m_CriticalSection);
	return dwCount;
}

const BOOL CBonTuner::GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	BYTE *pSrc = NULL;

	if(GetTsStream(&pSrc, pdwSize, pdwRemain)){
		if(*pdwSize){
			::CopyMemory(pDst, pSrc, *pdwSize);
		}
		return TRUE;
	}

	return FALSE;
}

const BOOL CBonTuner::GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	BOOL bRet;
	::EnterCriticalSection(&m_CriticalSection);
	if( m_hasStream && m_TsBuff.size() != 0 ){
		delete m_LastBuff;
		m_LastBuff = m_TsBuff.front();
		m_TsBuff.pop_front();
		*pdwSize = m_LastBuff->dwSize;
		*ppDst = m_LastBuff->pbBuff;
		*pdwRemain = (DWORD)m_TsBuff.size();
		bRet = TRUE;
	}else{
		*pdwSize = 0;
		*pdwRemain = 0;
		bRet = FALSE;
	}
	::LeaveCriticalSection(&m_CriticalSection);
	return bRet;
}

void CBonTuner::PurgeTsStream(void)
{
	//バッファ解放
	::EnterCriticalSection(&m_CriticalSection);
	while (!m_TsBuff.empty()){
		TS_DATA *p = m_TsBuff.front();
		m_TsBuff.pop_front();
		delete p;
	}
	::LeaveCriticalSection(&m_CriticalSection);
}

LPCTSTR CBonTuner::GetTunerName(void)
{
	return m_strTunerName.c_str();
}

const BOOL CBonTuner::IsTunerOpening(void)
{
	return FALSE;
}

LPCTSTR CBonTuner::EnumTuningSpace(const DWORD dwSpace)
{
	map<DWORD, SPACE_DATA>::iterator itr;
	itr = m_chSet.spaceMap.find(dwSpace);
	if( itr == m_chSet.spaceMap.end() ){
		return NULL;
	}else{
		return itr->second.wszName.c_str();
	}
}

LPCTSTR CBonTuner::EnumChannelName(const DWORD dwSpace, const DWORD dwChannel)
{
	DWORD key = dwSpace<<16 | dwChannel;
	map<DWORD, CH_DATA>::iterator itr;
	itr = m_chSet.chMap.find(key);
	if( itr == m_chSet.chMap.end() ){
		return NULL;
	}else{
		return itr->second.wszName.c_str();
	}
}

const BOOL CBonTuner::SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
	DWORD key = dwSpace<<16 | dwChannel;
	map<DWORD, CH_DATA>::iterator itr;
	itr = m_chSet.chMap.find(key);
	if( itr == m_chSet.chMap.end() ){
		return FALSE;
	}

	m_hasStream=FALSE ;

	DWORD dwRet=CMD_ERR;
	if( m_iID != -1 ){
		dwRet=SendSetCh(m_iID, itr->second.dwPT1Ch, itr->second.dwTSID);
	}else{
		return FALSE;
	}

	PurgeTsStream();

	m_hasStream = (dwRet&CMD_BIT_NON_STREAM) ? FALSE : TRUE ;
	dwRet &= ~CMD_BIT_NON_STREAM ;

	if( dwRet==CMD_SUCCESS ){
		m_dwCurSpace = dwSpace;
		m_dwCurChannel = dwChannel;
		return TRUE;
	}

	return FALSE;
}

const DWORD CBonTuner::GetCurSpace(void)
{
	return m_dwCurSpace;
}

const DWORD CBonTuner::GetCurChannel(void)
{
	return m_dwCurChannel;
}

void CBonTuner::Release()
{
	delete this;
}

UINT WINAPI CBonTuner::RecvThread(LPVOID pParam)
{
	CBonTuner* pSys = (CBonTuner*)pParam;

	wstring strEvent;
	wstring strPipe;
	Format(strEvent, L"%s%d", CMD_PT1_DATA_EVENT_WAIT_CONNECT, pSys->m_iID);
	Format(strPipe, L"%s%d", CMD_PT1_DATA_PIPE, pSys->m_iID);

	while (1) {
		if (::WaitForSingleObject( pSys->m_hStopEvent, 0 ) != WAIT_TIMEOUT) {
			//中止
			break;
		}
		DWORD dwSize;
		BYTE *pbBuff;
		if ((SendSendData(pSys->m_iID, &pbBuff, &dwSize, strEvent, strPipe) == CMD_SUCCESS) && (dwSize != 0)) {
			if(pSys->m_hasStream) {
				TS_DATA *pData = new TS_DATA(pbBuff, dwSize);
				::EnterCriticalSection(&pSys->m_CriticalSection);
				while (pSys->m_TsBuff.size() > MAX_BUFF_COUNT) {
					TS_DATA *p = pSys->m_TsBuff.front();
					pSys->m_TsBuff.pop_front();
					delete p;
				}
				pSys->m_TsBuff.push_back(pData);
				::LeaveCriticalSection(&pSys->m_CriticalSection);
				::SetEvent(pSys->m_hOnStreamEvent);
			}else {
				//休止
				delete [] pbBuff ;
			}
		}else{
			if(!pSys->m_hasStream) pSys->PurgeTsStream();
			::Sleep(5);
		}
	}

	return 0;
}
