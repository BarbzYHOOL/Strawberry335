/*
 * Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "InstanceScript.h"
#include "eye_of_eternity.h"
#include "Player.h"

class instance_eye_of_eternity : public InstanceMapScript
{
    public:

    instance_eye_of_eternity() : InstanceMapScript("instance_eye_of_eternity", 616) { }

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_eye_of_eternity_InstanceMapScript(map);
    }

    struct instance_eye_of_eternity_InstanceMapScript : public InstanceScript
    {
        instance_eye_of_eternity_InstanceMapScript(Map* map) : InstanceScript(map)
        {
            SetBossNumber(MAX_ENCOUNTER);
        }

        typedef std::vector<uint64> GuidList;

        void Initialize() override
        {
            malygosGUID = 0;
            irisGUID = 0;
            lastPortalGUID = 0;
            platformGUID = 0;
            exitPortalGUID = 0;
            heartOfMagicGUID = 0;
            alexstraszaBunnyGUID = 0;
            giftBoxBunnyGUID = 0;
        }

        bool SetBossState(uint32 type, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            if (type == DATA_MALYGOS_EVENT)
            {
                if (state == FAIL)
                {
                    for (GuidList::const_iterator itr_trigger = portalTriggers.begin(); itr_trigger != portalTriggers.end(); ++itr_trigger)
                    {
                        if (Creature* trigger = instance->GetCreature(*itr_trigger))
                        {
                            // just in case
                            trigger->RemoveAllAuras();
                            trigger->AI()->Reset();
                        }
                    }

                    if (GameObject* platform = instance->GetGameObject(platformGUID))
                        platform->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_DESTROYED);

                    SpawnGameObject(GO_EXIT_PORTAL, exitPortalPosition);
                    SpawnGameObject(instance->GetDifficulty() == RAID_DIFFICULTY_10MAN_NORMAL ? GO_FOCUSING_IRIS_10 : GO_FOCUSING_IRIS_25, focusingIrisPosition);
                }
                else if (state == DONE)
                    SpawnGameObject(GO_EXIT_PORTAL, exitPortalPosition);
            }
            return true;
        }

        /// @todo this should be handled in map, maybe add a summon function in map
        // There is no other way afaik...
        void SpawnGameObject(uint32 entry, Position& pos)
        {
            GameObject* go = new GameObject;
            if (!go->Create(sObjectMgr->GenerateLowGuid(HIGHGUID_GAMEOBJECT), entry, instance,
                PHASEMASK_NORMAL, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(),
                0, 0, 0, 0, 120, GO_STATE_READY))
            {
                delete go;
                return;
            }

            instance->AddToMap(go);
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            switch (go->GetEntry())
            {
                case GO_NEXUS_RAID_PLATFORM:
                    platformGUID = go->GetGUID();
                    break;
                case GO_FOCUSING_IRIS_10:
                    if (instance->GetDifficulty() == RAID_DIFFICULTY_10MAN_NORMAL)
                    {
                        irisGUID = go->GetGUID();
                        focusingIrisPosition = go->GetPosition();
                    }
                    break;
                case GO_FOCUSING_IRIS_25:
                    if (instance->GetDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL)
                    {
                        irisGUID = go->GetGUID();
                        focusingIrisPosition = go->GetPosition();
                    }
                    break;
                case GO_EXIT_PORTAL:
                    exitPortalGUID = go->GetGUID();
                    exitPortalPosition = go->GetPosition();
                    break;
                case GO_HEART_OF_MAGIC_10:
                    if (instance->GetDifficulty() == RAID_DIFFICULTY_10MAN_NORMAL)
                        heartOfMagicGUID = go->GetGUID();
                    break;
                case GO_HEART_OF_MAGIC_25:
                    if (instance->GetDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL)
                        heartOfMagicGUID = go->GetGUID();
                    break;
            }
        }

        void OnCreatureCreate(Creature* creature) override
        {
            switch (creature->GetEntry())
            {
                case NPC_VORTEX_TRIGGER:
                    vortexTriggers.push_back(creature->GetGUID());
                    break;
                case NPC_MALYGOS:
                    malygosGUID = creature->GetGUID();
                    break;
                case NPC_PORTAL_TRIGGER:
                    portalTriggers.push_back(creature->GetGUID());
                    break;
                case NPC_ALEXSTRASZA_BUNNY:
                    alexstraszaBunnyGUID = creature->GetGUID();
                    break;
                case NPC_ALEXSTRASZAS_GIFT:
                    giftBoxBunnyGUID = creature->GetGUID();
                    break;
            }
        }

        void OnUnitDeath(Unit* unit) override
        {
            if (unit->GetTypeId() != TYPEID_PLAYER)
                return;

            // Player continues to be moving after death no matter if spline will be cleared along with all movements,
            // so on next world tick was all about delay if box will pop or not (when new movement will be registered)
            // since in EoE you never stop falling. However root at this precise* moment works,
            // it will get cleared on release. If by any chance some lag happen "Reload()" and "RepopMe()" works,
            // last test I made now gave me 50/0 of this bug so I can't do more about it.
            unit->SetControlled(true, UNIT_STATE_ROOT);
        }

        void ProcessEvent(WorldObject* /*obj*/, uint32 eventId) override
        {
            if (eventId == EVENT_FOCUSING_IRIS)
            {
                if (Creature* alexstraszaBunny = instance->GetCreature(alexstraszaBunnyGUID))
                {
                    alexstraszaBunny->CastSpell(alexstraszaBunny, SPELL_IRIS_OPENED);
                    instance->GetGameObject(irisGUID)->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
                }

                if (Creature* malygos = instance->GetCreature(malygosGUID))
                    malygos->AI()->DoAction(0); // ACTION_LAND_ENCOUNTER_START

                if (GameObject* exitPortal = instance->GetGameObject(exitPortalGUID))
                    exitPortal->Delete();
            }
        }

        void VortexHandling()
        {
            if (Creature* malygos = instance->GetCreature(malygosGUID))
            {
                std::list<HostileReference*> m_threatlist = malygos->getThreatManager().getThreatList();
                for (GuidList::const_iterator itr_vortex = vortexTriggers.begin(); itr_vortex != vortexTriggers.end(); ++itr_vortex)
                {
                    if (m_threatlist.empty())
                        return;

                    uint8 counter = 0;
                    if (Creature* trigger = instance->GetCreature(*itr_vortex))
                    {
                        // each trigger have to cast the spell to 5 players.
                        for (std::list<HostileReference*>::const_iterator itr = m_threatlist.begin(); itr!= m_threatlist.end(); ++itr)
                        {
                            if (counter >= 5)
                                break;

                            if (Unit* target = (*itr)->getTarget())
                            {
                                Player* player = target->ToPlayer();

                                if (!player || player->isGameMaster() || player->HasAura(SPELL_VORTEX_4))
                                    continue;

                                player->CastSpell(trigger, SPELL_VORTEX_4, true);
                                counter++;
                            }
                        }
                    }
                }
            }
        }

        void PowerSparksHandling()
        {
            bool next = (lastPortalGUID == portalTriggers.back() || !lastPortalGUID ? true : false);

            for (GuidList::const_iterator itr_trigger = portalTriggers.begin(); itr_trigger != portalTriggers.end(); ++itr_trigger)
            {
                if (next)
                {
                    if (Creature* trigger = instance->GetCreature(*itr_trigger))
                    {
                        lastPortalGUID = trigger->GetGUID();
                        trigger->CastSpell(trigger, SPELL_PORTAL_OPENED, true);
                        return;
                    }
                }

                if (*itr_trigger == lastPortalGUID)
                    next = true;
            }
        }

        void SetData(uint32 data, uint32 /*value*/) override
        {
            switch (data)
            {
                case DATA_VORTEX_HANDLING:
                    VortexHandling();
                    break;
                case DATA_POWER_SPARKS_HANDLING:
                    PowerSparksHandling();
                    break;
            }
        }

        uint64 GetData64(uint32 data) override
        {
            switch (data)
            {
                case DATA_TRIGGER:
                    return vortexTriggers.front();
                case DATA_MALYGOS:
                    return malygosGUID;
                case DATA_PLATFORM:
                    return platformGUID;
                case DATA_ALEXSTRASZA_BUNNY_GUID:
                    return alexstraszaBunnyGUID;
                case DATA_HEART_OF_MAGIC_GUID:
                    return heartOfMagicGUID;
                case DATA_FOCUSING_IRIS_GUID:
                    return irisGUID;
                case DATA_GIFT_BOX_BUNNY_GUID:
                    return giftBoxBunnyGUID;
            }

            return NULL;
        }

        std::string GetSaveData() override
        {
            OUT_SAVE_INST_DATA;

            std::ostringstream saveStream;
            saveStream << "E O E " << GetBossSaveData();

            OUT_SAVE_INST_DATA_COMPLETE;

            return saveStream.str();
        }

        void Load(const char* in) override
        {
            if (!in)
            {
                OUT_LOAD_INST_DATA_FAIL;
                return;
            }

            OUT_LOAD_INST_DATA(in);

            char dataHead1, dataHead2, dataHead3;
            uint16 data;

            std::istringstream loadStream(in);
            loadStream >> dataHead1 >> dataHead2 >> dataHead3 >> data;

            if (dataHead1 == 'E' && dataHead2 == 'O'  && dataHead3 == 'E')
            {
                if (data != NOT_STARTED && data != DONE)
                    data = NOT_STARTED;

                SetBossState(DATA_MALYGOS_EVENT, EncounterState(data));
            }
            else
                OUT_LOAD_INST_DATA_FAIL;

            OUT_LOAD_INST_DATA_COMPLETE;
        }

    private:

        GuidList vortexTriggers;
        GuidList portalTriggers;

        uint64 malygosGUID;
        uint64 irisGUID;
        uint64 lastPortalGUID;
        uint64 platformGUID;
        uint64 exitPortalGUID;
        uint64 heartOfMagicGUID;
        uint64 alexstraszaBunnyGUID;
        uint64 giftBoxBunnyGUID;

        Position focusingIrisPosition;
        Position exitPortalPosition;
    };
};

void AddSC_instance_eye_of_eternity()
{
    new instance_eye_of_eternity();
}