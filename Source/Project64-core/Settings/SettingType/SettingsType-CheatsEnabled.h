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
#pragma once

#include <Project64-core/Settings/SettingType/SettingsType-Base.h>
#include <Common/IniFileClass.h>

class CSettingTypeCheatsEnabled :
    public CSettingType
{
public:
    CSettingTypeCheatsEnabled(const char * PostFix, bool DefaultValue);
    ~CSettingTypeCheatsEnabled();

    virtual bool        IndexBasedSetting ( void ) const { return true; }
    virtual SettingType GetSettingType    ( void ) const { return SettingType_CheatSetting; }
    virtual bool        IsSettingSet      ( void ) const;

    //return the values
    virtual bool Load   ( int32_t Index, bool & Value   ) const;
    virtual bool Load   ( int32_t Index, uint32_t & Value  ) const;
    virtual bool Load   ( int32_t Index, stdstr & Value ) const;

    //return the default values
    virtual void LoadDefault ( int32_t Index, bool & Value   ) const;
    virtual void LoadDefault ( int32_t Index, uint32_t & Value  ) const;
    virtual void LoadDefault ( int32_t Index, stdstr & Value ) const;

    //Update the settings
    virtual void Save   ( int32_t Index, bool Value );
    virtual void Save   ( int32_t Index, uint32_t Value );
    virtual void Save   ( int32_t Index, const stdstr & Value );
    virtual void Save   ( int32_t Index, const char * Value );

    // Delete the setting
    virtual void Delete ( int32_t Index );

    // Initialize this class to use ini or registry
    static void Initialize   ( void );
    static void CleanUp      ( void );
    static void FlushChanges ( void );

protected:
    static CIniFile * m_EnabledIniFile;
    static stdstr   * m_SectionIdent;
    const char * const m_PostFix;
    const bool m_DefaultValue;
    static void GameChanged ( void * /*Data */ );

private:
    CSettingTypeCheatsEnabled(void);                                        // Disable default constructor
    CSettingTypeCheatsEnabled(const CSettingTypeCheatsEnabled&);            // Disable copy constructor
    CSettingTypeCheatsEnabled& operator=(const CSettingTypeCheatsEnabled&); // Disable assignment
};
