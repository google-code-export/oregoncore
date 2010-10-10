/* Copyright (C) 2006 - 2008 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* ScriptData
SDName: Boss_Kalecgos
SD%Complete: 95
SDComment:
SDCategory: Sunwell_Plateau
EndScriptData */

#include "ScriptPCH.h"
#include "sunwell_plateau.h"

enum Yells
{
    //Kalecgos dragon form
    SAY_EVIL_AGGRO                               = -1580000,
    SAY_EVIL_SPELL1                              = -1580001,
    SAY_EVIL_SPELL2                              = -1580002,
    SAY_EVIL_SLAY1                               = -1580003,
    SAY_EVIL_SLAY2                               = -1580004,
    SAY_EVIL_ENRAGE                              = -1580005,

    //Kalecgos humanoid form
    SAY_GOOD_AGGRO                               = -1580006,
    SAY_GOOD_NEAR_DEATH                          = -1580007,
    SAY_GOOD_NEAR_DEATH2                         = -1580008,
    SAY_GOOD_PLRWIN                              = -1580009,

    //Sathrovarr
    SAY_SATH_AGGRO                               = -1580010,
    SAY_SATH_DEATH                               = -1580011,
    SAY_SATH_SPELL1                              = -1580012,
    SAY_SATH_SPELL2                              = -1580013,
    SAY_SATH_SLAY1                               = -1580014,
    SAY_SATH_SLAY2                               = -1580015,
    SAY_SATH_ENRAGE                              = -1580016,
};

enum Spells
{
    AURA_SUNWELL_RADIANCE                        = 45769,
    AURA_SPECTRAL_EXHAUSTION                     = 44867,
    AURA_SPECTRAL_REALM                          = 46021,
    AURA_SPECTRAL_INVISIBILITY                   = 44801,
    AURA_DEMONIC_VISUAL                          = 44800,

    SPELL_SPECTRAL_BLAST                         = 44869,
    SPELL_TELEPORT_SPECTRAL                      = 46019,
    SPELL_ARCANE_BUFFET                          = 45018,
    SPELL_FROST_BREATH                           = 44799,
    SPELL_TAIL_LASH                              = 45122,

    SPELL_BANISH                                 = 44836,
    SPELL_TRANSFORM_KALEC                        = 44670,
    SPELL_ENRAGE                                 = 44807,

    SPELL_CORRUPTION_STRIKE                      = 45029,
    SPELL_AGONY_CURSE                            = 45032,
    SPELL_SHADOW_BOLT                            = 45031,

    SPELL_HEROIC_STRIKE                          = 45026,
    SPELL_REVITALIZE                             = 45027
};

#define GO_FAILED   "You are unable to use this currently."

#define FLY_X   1679
#define FLY_Y   900
#define FLY_Z   82

#define CENTER_X    1705
#define CENTER_Y    930
#define RADIUS      30

#define DRAGON_REALM_Z  53.079
#define DEMON_REALM_Z   -74.558

uint32 WildMagic[] = { 44978, 45001, 45002, 45004, 45006, 45010 };

struct boss_kalecgosAI : public ScriptedAI
{
    boss_kalecgosAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
        SathGUID = 0;
    }

    ScriptedInstance *pInstance;

    uint32 ArcaneBuffetTimer;
    uint32 FrostBreathTimer;
    uint32 WildMagicTimer;
    uint32 SpectralBlastTimer;
    uint32 TailLashTimer;
    uint32 CheckTimer;
    uint32 TalkTimer;
    uint32 TalkSequence;

    bool isFriendly;
    bool isEnraged;
    bool isBanished;

    uint64 SathGUID;

    void Reset()
    {
        if (pInstance)
        {
            SathGUID = pInstance->GetData64(DATA_SATHROVARR);
            pInstance->SetData(DATA_KALECGOS_EVENT, NOT_STARTED);
        }

        if (Creature *Sath = Unit::GetCreature(*me, SathGUID))
            Sath->AI()->EnterEvadeMode();

        me->setFaction(14);
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE + UNIT_FLAG_NOT_SELECTABLE);
        me->RemoveUnitMovementFlag(MOVEFLAG_ONTRANSPORT | MOVEFLAG_LEVITATING);
        me->SetVisibility(VISIBILITY_ON);
        me->SetStandState(UNIT_STAND_STATE_SLEEP);

        ArcaneBuffetTimer = 8000;
        FrostBreathTimer = 15000;
        WildMagicTimer = 10000;
        TailLashTimer = 25000;
        SpectralBlastTimer = 20000+(rand()%5000);
        CheckTimer = SpectralBlastTimer+20000; //after spectral blast

        TalkTimer = 0;
        TalkSequence = 0;
        isFriendly = false;
        isEnraged = false;
        isBanished = false;
    }

    void DamageTaken(Unit *done_by, uint32 &damage)
    {
        if (damage >= me->GetHealth() && done_by != me)
            damage = 0;
    }

    void EnterCombat(Unit* /*who*/)
    {
        me->SetStandState(UNIT_STAND_STATE_STAND);
        DoScriptText(SAY_EVIL_AGGRO, me);
        DoZoneInCombat();

        if (pInstance)
            pInstance->SetData(DATA_KALECGOS_EVENT, IN_PROGRESS);
    }

    void KilledUnit(Unit * /*victim*/)
    {
        switch(rand()%2)
        {
        case 0: DoScriptText(SAY_EVIL_SLAY1, me); break;
        case 1: DoScriptText(SAY_EVIL_SLAY2, me); break;
        }
    }

    void MovementInform(uint32 type,uint32 /*id*/)
    {
        if (type != POINT_MOTION_TYPE)
            return;

        me->SetVisibility(VISIBILITY_OFF);
        if (isFriendly)
            me->setDeathState(JUST_DIED);
        else
        {
            me->GetMotionMaster()->MoveTargetedHome();
            TalkTimer = 30000;
        }
    }

    void GoodEnding()
    {
        switch(TalkSequence)
        {
        case 1:
            me->setFaction(35);
            TalkTimer = 1000;
            break;
        case 2:
            DoScriptText(SAY_GOOD_PLRWIN, me);
            TalkTimer = 10000;
            break;
        case 3:
            me->AddUnitMovementFlag(MOVEFLAG_ONTRANSPORT | MOVEFLAG_LEVITATING);
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MovePoint(1,FLY_X,FLY_Y,FLY_Z);
            TalkTimer = 600000;
            break;
        default:
            break;
        }
    }

    void BadEnding()
    {
        switch(TalkSequence)
        {
        case 1:
            DoScriptText(SAY_EVIL_ENRAGE, me);
            TalkTimer = 3000;
            break;
        case 2:
            me->AddUnitMovementFlag(MOVEFLAG_ONTRANSPORT | MOVEFLAG_LEVITATING);
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MovePoint(1,FLY_X,FLY_Y,FLY_Z);
            TalkTimer = 600000;
            break;
        case 3:
            EnterEvadeMode();
            break;
        default:
            break;
        }
    }

    void UpdateAI(const uint32 diff);
};

struct boss_sathrovarrAI : public ScriptedAI
{
    boss_sathrovarrAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
        KalecGUID = 0;
        KalecgosGUID = 0;
    }

    ScriptedInstance *pInstance;

    uint32 CorruptionStrikeTimer;
    uint32 AgonyCurseTimer;
    uint32 ShadowBoltTimer;
    uint32 CheckTimer;
    uint32 ResetThreat;

    uint64 KalecGUID;
    uint64 KalecgosGUID;

    bool isEnraged;
    bool isBanished;

    void Reset()
    {
        if (pInstance)
        {
            KalecgosGUID = pInstance->GetData64(DATA_KALECGOS_DRAGON);
            pInstance->SetData(DATA_KALECGOS_EVENT, NOT_STARTED);
        }
        if (KalecGUID)
        {
            if (Creature* Kalec = Unit::GetCreature(*me, KalecGUID))
                Kalec->setDeathState(JUST_DIED);
            KalecGUID = 0;
        }

        ShadowBoltTimer = 7000 + rand()%3 * 1000;
        AgonyCurseTimer = 20000;
        CorruptionStrikeTimer = 13000;
        CheckTimer = 1000;
        ResetThreat = 1000;
        isEnraged = false;
        isBanished = false;
    }

    void EnterCombat(Unit* /*who*/)
    {
        if (Creature *Kalec = me->SummonCreature(MOB_KALEC, me->GetPositionX() + 10, me->GetPositionY() + 5, me->GetPositionZ(), 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 0))
        {
            KalecGUID = Kalec->GetGUID();
            me->CombatStart(Kalec);
            me->AddThreat(Kalec, 100.0f);
        }
        DoScriptText(SAY_SATH_AGGRO, me);
    }

    void DamageTaken(Unit *done_by, uint32 &damage)
    {
        if (damage >= me->GetHealth() && done_by != me)
            damage = 0;
    }

    void KilledUnit(Unit *pTarget)
    {
        if (pTarget->GetGUID() == KalecGUID)
        {
            TeleportAllPlayersBack();
            if (Creature *Kalecgos = Unit::GetCreature(*me, KalecgosGUID))
            {
                CAST_AI(boss_kalecgosAI, Kalecgos->AI())->TalkTimer = 1;
                CAST_AI(boss_kalecgosAI, Kalecgos->AI())->isFriendly = false;
            }
            EnterEvadeMode();
            return;
        }
        switch(rand()%2)
        {
        case 0: DoScriptText(SAY_SATH_SLAY1, me); break;
        case 1: DoScriptText(SAY_SATH_SLAY2, me); break;
        }
    }

    void JustDied(Unit * /*killer*/)
    {
        DoScriptText(SAY_SATH_DEATH, me);
        me->Relocate(me->GetPositionX(), me->GetPositionY(), DRAGON_REALM_Z, me->GetOrientation());
        TeleportAllPlayersBack();
        if (Creature *Kalecgos = Unit::GetCreature(*me, KalecgosGUID))
        {
            CAST_AI(boss_kalecgosAI, Kalecgos->AI())->TalkTimer = 1;
            CAST_AI(boss_kalecgosAI, Kalecgos->AI())->isFriendly = true;
        }

        if (pInstance)
            pInstance->SetData(DATA_KALECGOS_EVENT, DONE);
    }

    void TeleportAllPlayersBack()
    {
        Map* pMap = me->GetMap();
        if (!pMap->IsDungeon()) return;
        Map::PlayerList const &PlayerList = pMap->GetPlayers();
        for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
        {
            if (Player* i_pl = i->getSource())
                if (i_pl->HasAura(AURA_SPECTRAL_REALM,0))
                    i_pl->RemoveAurasDueToSpell(AURA_SPECTRAL_REALM);
        }
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        if (CheckTimer <= diff)
        {
            if (((me->GetHealth()*100 / me->GetMaxHealth()) < 10) && !isEnraged)
            {
                Unit* Kalecgos = Unit::GetUnit(*me, KalecgosGUID);
                if (Kalecgos)
                {
                    Kalecgos->CastSpell(Kalecgos, SPELL_ENRAGE, true);
                    ((boss_kalecgosAI*)CAST_CRE(Kalecgos)->AI())->isEnraged = true;
                }
                DoCast(me, SPELL_ENRAGE, true);
                isEnraged = true;
            }

            if (!isBanished && (me->GetHealth()*100)/me->GetMaxHealth() < 1)
            {
                if (Unit *Kalecgos = Unit::GetUnit(*me, KalecgosGUID))
                {
                    if (((boss_kalecgosAI*)CAST_CRE(Kalecgos)->AI())->isBanished)
                    {
                        me->DealDamage(me, me->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                        return;
                    }
                    else
                    {
                        DoCast(me, SPELL_BANISH);
                        isBanished = true;
                    }
                }
                else
                {
                    DoTextEmote("is unable to find Kalecgos", NULL);
                    EnterEvadeMode();
                    return;
                }
            }
            CheckTimer = 1000;
        } else CheckTimer -= diff;

        if (ResetThreat <= diff)
        {
            if ((me->getVictim()->HasAura(AURA_SPECTRAL_EXHAUSTION,0)) && (me->getVictim()->GetTypeId() == TYPEID_PLAYER))
            {
                for (std::list<HostileReference*>::iterator itr = me->getThreatManager().getThreatList().begin(); itr != me->getThreatManager().getThreatList().end(); ++itr)
                {
                    if (((*itr)->getUnitGuid()) == (me->getVictim()->GetGUID()))
                    {
                        (*itr)->removeReference();
                        break;
                    }
                }
            }
            ResetThreat = 1000;
        } else ResetThreat -= diff;

        if (ShadowBoltTimer <= diff)
        {
            DoScriptText(SAY_SATH_SPELL1, me);
            DoCast(me, SPELL_SHADOW_BOLT);
            ShadowBoltTimer = 7000+(rand()%3000);
        } else ShadowBoltTimer -= diff;

        if (AgonyCurseTimer <= diff)
        {
            Unit *pTarget = SelectUnit(SELECT_TARGET_RANDOM, 0);
            if (!pTarget) pTarget = me->getVictim();
            DoCast(pTarget, SPELL_AGONY_CURSE);
            AgonyCurseTimer = 20000;
        } else AgonyCurseTimer -= diff;

        if (CorruptionStrikeTimer <= diff)
        {
            DoScriptText(SAY_SATH_SPELL2, me);
            DoCast(me->getVictim(), SPELL_CORRUPTION_STRIKE);
            CorruptionStrikeTimer = 13000;
        } else CorruptionStrikeTimer -= diff;

        DoMeleeAttackIfReady();
    }
};

struct boss_kalecAI : public ScriptedAI
{
    ScriptedInstance *pInstance;

    uint32 RevitalizeTimer;
    uint32 HeroicStrikeTimer;
    uint32 YellTimer;
    uint32 YellSequence;

    uint64 SathGUID;

    bool isEnraged; // if demon is enraged

    boss_kalecAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
    }

    void Reset()
    {
        if (pInstance)
            SathGUID = pInstance->GetData64(DATA_SATHROVARR);

        RevitalizeTimer = 5000;
        HeroicStrikeTimer = 3000;
        YellTimer = 5000;
        YellSequence = 0;

        isEnraged = false;
    }

    void DamageTaken(Unit *done_by, uint32 &damage)
    {
        if (done_by->GetGUID() != SathGUID)
            damage = 0;
        else if (isEnraged)
            damage *= 3;
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        if (YellTimer <= diff)
        {
            switch(YellSequence)
            {
            case 0:
                DoScriptText(SAY_GOOD_AGGRO, me);
                ++YellSequence;
                break;
            case 1:
                if ((me->GetHealth()*100)/me->GetMaxHealth() < 50)
                {
                    DoScriptText(SAY_GOOD_NEAR_DEATH, me);
                    ++YellSequence;
                }
                break;
            case 2:
                if ((me->GetHealth()*100)/me->GetMaxHealth() < 10)
                {
                    DoScriptText(SAY_GOOD_NEAR_DEATH2, me);
                    ++YellSequence;
                }
                break;
            default:
                break;
            }
            YellTimer = 5000;
        }

        if (RevitalizeTimer <= diff)
        {
            DoCast(me, SPELL_REVITALIZE);
            RevitalizeTimer = 5000;
        } else RevitalizeTimer -= diff;

        if (HeroicStrikeTimer <= diff)
        {
            DoCast(me->getVictim(), SPELL_HEROIC_STRIKE);
            HeroicStrikeTimer = 2000;
        } else HeroicStrikeTimer -= diff;

        DoMeleeAttackIfReady();
    }
};

void boss_kalecgosAI::UpdateAI(const uint32 diff)
{
    if (TalkTimer)
    {
        if (!TalkSequence)
        {
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE + UNIT_FLAG_NOT_SELECTABLE);
            me->InterruptNonMeleeSpells(true);
            me->RemoveAllAuras();
            me->DeleteThreatList();
            me->CombatStop();
            TalkSequence++;
        }
        if (TalkTimer <= diff)
        {
            if (isFriendly)
                GoodEnding();
            else
                BadEnding();
            TalkSequence++;
        } else TalkTimer -= diff;
    }
    else
    {
        if (!UpdateVictim())
            return;

        if (CheckTimer <= diff)
         {
             if (((me->GetHealth()*100 / me->GetMaxHealth()) < 10) && !isEnraged)
             {
                 Unit* Sath = Unit::GetUnit(*me, SathGUID);
                 if (Sath)
                 {
                     Sath->CastSpell(Sath, SPELL_ENRAGE, true);
                     ((boss_sathrovarrAI*)CAST_CRE(Sath)->AI())->isEnraged = true;
                 }
                 DoCast(me, SPELL_ENRAGE, true);
                 isEnraged = true;
             }

             if (!isBanished && (me->GetHealth()*100)/me->GetMaxHealth() < 1)
             {
                 if (Unit *Sath = Unit::GetUnit(*me, SathGUID))
                 {
                     if (((boss_sathrovarrAI*)CAST_CRE(Sath)->AI())->isBanished)
                     {
                         Sath->DealDamage(Sath, Sath->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                         return;
                     }
                     else
                     {
                         DoCast(me, SPELL_BANISH);
                         isBanished = true;
                     }
                 }
                 else
                 {
                     error_log("OSCR: Didn't find Shathrowar. Kalecgos event reseted.");
                     EnterEvadeMode();
                     return;
                 }
             }
             CheckTimer = 1000;
        } else CheckTimer -= diff;

        if (ArcaneBuffetTimer <= diff)
        {
            DoCastAOE(SPELL_ARCANE_BUFFET);
            ArcaneBuffetTimer = 8000;
        } else ArcaneBuffetTimer -= diff;

        if (FrostBreathTimer <= diff)
        {
            DoCastAOE(SPELL_FROST_BREATH);
            FrostBreathTimer = 15000;
        } else FrostBreathTimer -= diff;

        if (TailLashTimer <= diff)
        {
            DoCastAOE(SPELL_TAIL_LASH);
            TailLashTimer = 15000;
        } else TailLashTimer -= diff;

        if (WildMagicTimer <= diff)
        {
            DoCastAOE(WildMagic[rand()%6]);
            WildMagicTimer = 20000;
        } else WildMagicTimer -= diff;

        if (SpectralBlastTimer <= diff)
        {
            //this is a hack. we need to find a victim without aura in core
            Unit *pTarget = SelectUnit(SELECT_TARGET_RANDOM, 0);
            if ((pTarget && pTarget != me->getVictim()) && pTarget->isAlive() && !(pTarget->HasAura(AURA_SPECTRAL_EXHAUSTION, 0)))
            {
                DoCast(pTarget, SPELL_SPECTRAL_BLAST);
                SpectralBlastTimer = 20000+(rand()%5000);
            }
            else
            {
                SpectralBlastTimer = 1000;
            }
        } else SpectralBlastTimer -= diff;

        DoMeleeAttackIfReady();
    }
}

bool GOkalecgos_teleporter(Player *player, GameObject* _GO)
{
    if (player->HasAura(AURA_SPECTRAL_EXHAUSTION, 0))
        player->GetSession()->SendNotification(GO_FAILED);
    else
        player->CastSpell(player, SPELL_TELEPORT_SPECTRAL, true);
    return true;
}

CreatureAI* GetAI_boss_kalecgos(Creature* pCreature)
{
    return new boss_kalecgosAI (pCreature);
}

CreatureAI* GetAI_boss_Sathrovarr(Creature* pCreature)
{
    return new boss_sathrovarrAI (pCreature);
}

CreatureAI* GetAI_boss_kalec(Creature* pCreature)
{
    return new boss_kalecAI (pCreature);
}

void AddSC_boss_kalecgos()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_kalecgos";
    newscript->GetAI = &GetAI_boss_kalecgos;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "boss_sathrovarr";
    newscript->GetAI = &GetAI_boss_Sathrovarr;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "boss_kalec";
    newscript->GetAI = &GetAI_boss_kalec;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "kalecgos_teleporter";
    newscript->pGOHello = &GOkalecgos_teleporter;
    newscript->RegisterSelf();
}
