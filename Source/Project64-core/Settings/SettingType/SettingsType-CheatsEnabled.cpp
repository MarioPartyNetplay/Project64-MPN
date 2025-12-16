/****************************************************************************
*                                                                           *
* Project64 - A Nintendo 64 emulator.                                      *
* http://www.pj64-emu.com/                                                  *
* Copyright (C) 2012 Project64. All rights reserved.                        *
*                                                                           *
* License:                                                                  *
* GNU/GPLv2 http://www.gnu.org/licenses/gpl-2.0.html                        *
*                                                                           *
****************************************************************************/
#include "stdafx.h"
#include "SettingsType-CheatsEnabled.h"

CIniFile * CSettingTypeCheatsEnabled::m_EnabledIniFile = NULL;
stdstr   * CSettingTypeCheatsEnabled::m_SectionIdent = NULL;

CSettingTypeCheatsEnabled::CSettingTypeCheatsEnabled(const char * PostFix, bool DefaultValue) :
    m_PostFix(PostFix),
    m_DefaultValue(DefaultValue)
{
}

CSettingTypeCheatsEnabled::~CSettingTypeCheatsEnabled(void)
{
}

void CSettingTypeCheatsEnabled::Initialize(void)
{
    WriteTrace(TraceAppInit, TraceDebug, "Start");
    m_EnabledIniFile = new CIniFile(g_Settings->LoadStringVal(SupportFile_CheatsEnabled).c_str());
    m_EnabledIniFile->SetAutoFlush(false);
    g_Settings->RegisterChangeCB(Game_IniKey, NULL, GameChanged);
    m_SectionIdent = new stdstr(g_Settings->LoadStringVal(Game_IniKey));
    GameChanged(NULL);
    WriteTrace(TraceAppInit, TraceDebug, "Done");
}

void CSettingTypeCheatsEnabled::CleanUp(void)
{
    if (m_EnabledIniFile)
    {
        m_EnabledIniFile->SetAutoFlush(true);
        delete m_EnabledIniFile;
        m_EnabledIniFile = NULL;
    }
    if (m_SectionIdent)
    {
        delete m_SectionIdent;
        m_SectionIdent = NULL;
    }
}

void CSettingTypeCheatsEnabled::FlushChanges(void)
{
    if (m_EnabledIniFile)
    {
        m_EnabledIniFile->FlushChanges();
    }
}

void CSettingTypeCheatsEnabled::GameChanged(void * /*Data */)
{
    *m_SectionIdent = g_Settings->LoadStringVal(Game_IniKey);
}

bool CSettingTypeCheatsEnabled::IsSettingSet(void) const
{
    if (m_EnabledIniFile == NULL)
    {
        return false;
    }
    uint32_t Value;
    stdstr_f Key("Cheat%d%s", 0, m_PostFix);
    return m_EnabledIniFile->GetNumber(m_SectionIdent->c_str(), Key.c_str(), 0, Value);
}

bool CSettingTypeCheatsEnabled::Load(int Index, bool & Value) const
{
    if (m_EnabledIniFile == NULL)
    {
        Value = m_DefaultValue;
        return false;
    }
    uint32_t IntValue;
    stdstr_f Key("Cheat%d%s", Index, m_PostFix);
    bool Found = m_EnabledIniFile->GetNumber(m_SectionIdent->c_str(), Key.c_str(), m_DefaultValue ? 1 : 0, IntValue);
    Value = (IntValue != 0);
    return Found;
}

bool CSettingTypeCheatsEnabled::Load(int Index, uint32_t & Value) const
{
    if (m_EnabledIniFile == NULL)
    {
        Value = m_DefaultValue ? 1 : 0;
        return false;
    }
    stdstr_f Key("Cheat%d%s", Index, m_PostFix);
    return m_EnabledIniFile->GetNumber(m_SectionIdent->c_str(), Key.c_str(), m_DefaultValue ? 1 : 0, Value);
}

bool CSettingTypeCheatsEnabled::Load(int Index, stdstr & Value) const
{
    if (m_EnabledIniFile == NULL)
    {
        return false;
    }
    stdstr_f Key("Cheat%d%s", Index, m_PostFix);
    return m_EnabledIniFile->GetString(m_SectionIdent->c_str(), Key.c_str(), "", Value);
}

//return the default values
void CSettingTypeCheatsEnabled::LoadDefault(int /*Index*/, bool & Value) const
{
    Value = m_DefaultValue;
}

void CSettingTypeCheatsEnabled::LoadDefault(int /*Index*/, uint32_t & Value) const
{
    Value = m_DefaultValue ? 1 : 0;
}

void CSettingTypeCheatsEnabled::LoadDefault(int /*Index*/, stdstr & /*Value*/) const
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

//Update the settings
void CSettingTypeCheatsEnabled::Save(int Index, bool Value)
{
    if (m_EnabledIniFile == NULL) { return; }

    stdstr_f Key("Cheat%d%s", Index, m_PostFix);
    m_EnabledIniFile->SaveNumber(m_SectionIdent->c_str(), Key.c_str(), Value ? 1 : 0);
}

void CSettingTypeCheatsEnabled::Save(int Index, uint32_t Value)
{
    if (m_EnabledIniFile == NULL) { return; }

    stdstr_f Key("Cheat%d%s", Index, m_PostFix);
    m_EnabledIniFile->SaveNumber(m_SectionIdent->c_str(), Key.c_str(), Value);
}

void CSettingTypeCheatsEnabled::Save(int Index, const stdstr & Value)
{
    if (m_EnabledIniFile == NULL) { return; }

    stdstr_f Key("Cheat%d%s", Index, m_PostFix);
    m_EnabledIniFile->SaveString(m_SectionIdent->c_str(), Key.c_str(), Value.c_str());
}

void CSettingTypeCheatsEnabled::Save(int Index, const char * Value)
{
    if (m_EnabledIniFile == NULL) { return; }

    stdstr_f Key("Cheat%d%s", Index, m_PostFix);
    m_EnabledIniFile->SaveString(m_SectionIdent->c_str(), Key.c_str(), Value);
}

void CSettingTypeCheatsEnabled::Delete(int Index)
{
    stdstr_f Key("Cheat%d%s", Index, m_PostFix);
    m_EnabledIniFile->SaveString(m_SectionIdent->c_str(), Key.c_str(), NULL);
}
