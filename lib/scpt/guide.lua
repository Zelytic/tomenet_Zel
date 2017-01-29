--[[ 'guide.lua'
 * Purpose: Provide meta information for in-client guide searching for key terms. - C. Blue
]]

guide_races = 18
guide_race = {
    "Human",
    "Half-Elf",
    "Elf",
    "Hobbit",
    "Gnome",
    "Dwarf",
    "Half-Orc",
    "Half-Troll",
    "Dunadan",
    "High-Elf",
    "Yeek",
    "Goblin",
    "Ent",
    "Draconian",
    "Kobold",
    "Dark-Elf",
    "Vampire",
    "Maia",
}

guide_classes = 14
guide_class = {
    "Warrior",
    "Istar",
    "Priest",
    "Rogue",
    "Mimic",
    "Archer",
    "Paladin",
    "Death Knight",
    "Hell Knight",
    "Ranger",
    "Adventurer",
    "Druid",
    "Shaman",
    "Runemaster",
    "Mindcrafter",
}

--don't count magic schools as skills for searching purpose
--guide_skills = 71
guide_skills = 74 - 1 - 11 - 15 - 6 - 1
guide_skill = {
    "+Combat",
    "+Weaponmastery",
    "+Sword-mastery",
    ".Critical-strike",
    ".Axe-mastery",
    ".Blunt-mastery",
    ".Polearm-mastery",
    ".Combat Stances",
    ".Dual-Wield",
    ".Martial Arts",
    ".Interception",
    "+Archery",
    ".Sling-mastery",
    ".Bow-mastery",
    ".Crossbow-mastery",
    ".Boomerang-mastery",
    "+Magic",
--[[
    "+Wizardry",
]]--
    ".Sorcery",
--[[
    ".Mana",
    ".Fire",
    ".Water",
    ".Air",
    ".Earth",
    ".Nature",
    ".Conveyance",
    ".Divination",
    ".Temporal",
    ".Udun",
    ".Mind",
]]--
    ".Spell-power",
--[[
    "+Prayers",
    ".Holy Offense",
    ".Holy Defense",
    ".Holy Curing",
    ".Holy Support",
    "+Occultism",
    ".Shadow",
    ".Spirit",
    ".Hereticism",
    "+Druidism",
    ".Arcane Lore",
    ".Physical Lore",
    "+Mindcraft",
    ".Psycho-Power",
    ".Attunement",
    ".Mental Intrusion",
]]--
    "+Runecraft",
--[[
    ".Light",
    ".Dark",
    ".Nexus",
    ".Nether",
    ".Chaos",
    ".Mana ",

    ".Astral Knowledge",
]]--
    "+Blood Magic",
    ".Necromancy",
    ".Traumaturgy",
    ".Aura of Fear",
    ".Shivering Aura",
    ".Aura of Death",
    ".Mimicry",
    ".Magic device",
    ".Anti-magic",
    "+Sneakiness",
    ".Stealth",
    ".Stealing",
    ".Backstabbing",
    ".Dodging",
    ".Calmness",
    ".Trapping",
    "+Health",
    ".Swimming",
    ".Climbing",
    ".Digging",
}

--(note: Sorcery isn't a school)
guide_schools = 28
guide_school = {
    "Wizardry -",
    "Mana",
    "Fire",
    "Water",
    "Air",
    "Earth",
    "Nature",
    "Conveyance",
    "Divination",
    "Temporal",
    "Udun",
    "Mind",
    "Prayers -",
    "Holy Offense",
    "Holy Defense",
    "Holy Curing",
    "Holy Support",
    "Occultism -",
    "Shadow",
    "Spirit",
    "Hereticism",
    "Druidism -" ,
    "Arcane Lore",
    "Physical Lore",
    "Mindcraft -",
    "Psycho-Power",
    "Attunement",
    "Mental Intrusion",
    "Astral Knowledge -",
}

guide_spells = 163
guide_spell = {
    "Manathrust",
    "Recharge",
    "Disperse Magic",
    "Remove Curses",
    "Disruption Shield",

    "Globe of Light",
    "Fire Bolt",
    "Elemental Shield",
    "Fiery Shield",
    "Firewall",
    "Fire Ball",
    "Fireflash",

    "Vapor",
    "Ent's Potion",
    "Frost Bolt",
    "Water Bolt",
    "Tidal Wave",
    "Frost Barrier",
    "Frost Ball",

    "Noxious Cloud",
    "Lightning Bolt",
    "Wings of Wind",
    "Thunderstorm",
    "Invisibility",

    "Dig",
    "Acid Bolt",
    "Stone Prison",
    "Strike",
    "Shake",

    "Healing",
    "Recovery",
    "Vermin Control",
    "Thunderstorm",
    "Regeneration",
    "Grow Trees",
    "Poison Blood",

    "Phase Door",
    "Disarm",
    "Teleport",
    "Teleport Away",
    "Recall",
    "Probability Travel",
    "Mass Warp",

    "Detect Monsters",
    "Identify",
    "Greater Identify",
    "Vision",
    "Sense Hidden",
    "Reveal Ways",

    "Magelock",
    "Slow Monster",
    "Essence of Speed",
    "Mass Warp",

    "Genocide",
    "Obliteration",
    "Wraithform",
    "Disenchantment Bolt",
    "Hellfire",

    "Confuse",
    "Stun",
    "Sense Minds",
    "Telekinesis I",

    "Curse",
    "Holy Light",
    "Exorcism",
    "Redemption",
    "Orb of Draining",
    "Doomed Grounds",
    "Earthquake",

    "Blessing",
    "Holy Resistance",
    "Protection from evil",
    "Dispel Magic",
    "Glyph of Warding",
    "Martyrdom",

    "Cure Wounds",
    "Healing",
    "Break Curses",
    "Cleansing Light",
    "Curing",
    "Restoration",
    "Faithful Focus",
    "Resurrection",
    "Soul Curing",

    "Remove Fear",
    "Holy Light",
    "Detect Evil",
    "Sense Monsters",
    "Sanctuary",
    "Satisfy Hunger",
    "Sense Surroundings",
    "Zeal",

    "Cause Fear",
    "Blindness",
    "Detect Invisible",
    "Poisonous Fog",
    "Veil of Night",
    "Shadow Bolt",
    "Aspect of Peril",
    "Darkness",
    "Shadow Gate",
    "Shadow Shroud",
    "Chaos Bolt",
    "Nether Bolt",
    "Drain Life",
    "Darkness Storm",

    "Cause Wounds",
    "Tame Fear",
    "Starlight",
    "Trance",
    "Lightning Bolt",
    "Spear of Light",
    "Lift Curses",
    "Ethereal Eye",
    "Possess",
    "Guardian Spirit",

    "Terror",
    "Ignore Fear",
    "Fire Bolt",
    "Wrathflame",
    "Demonic Strength",
    --"Chaos Bolt", --duplicate (Shadow)
    "Dark Meditation",
    "Levitation",
    "Robes of Havoc",
    "Blood Sacrifice",

    "Nature's Call",
    "Toxic Moisture",
    "Ancient Lore",
    "Garden of the Gods",
    "Call of the Forest",

    "Forest's Embrace",
    "Quickfeet",
    "Herbal Tea",
    "Extra Growth",
    "Focus",

    "Psychic Hammer",
    "Psychokinesis",
    "Autokinesis I",
    "Autokinesis II",
    "Autokinesis III",
    "Feedback",
    "Pyrokinesis",
    "Cryokinesis",
    "Psychic Warp",
    "Telekinesis II",
    "Kinetic Shield",

    "Clear Mind",
    "Willpower",
    "Self-Reflection",
    "Accelerate Nerves",
    "Telepathy",
    "Recognition",
    "Stabilize Thoughts",

    "Psionic Blast",
    "Psi Storm",
    "Scare",
    "Confuse",
    "Hypnosis",
    "Drain Strength",
    "Psychic Suppression",
    "Remote Vision",
    "Recognition",
    "Charm",

    "Power Bolt",
    "Power Ray",
    "Power Blast",
    "Relocation",
    "Vengeance",
    "Empowerment",
    "The Silent Force",
    "Sphere of Destruction",
    "Gateway",
}
