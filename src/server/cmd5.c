/* $Id$ */
/* File: cmd5.c */

/* Purpose: Spell/Prayer commands */

/*
 * Copyright (c) 1989 James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research, and
 * not for profit purposes provided that this copyright and statement are
 * included in all such copies.
 */

#define SERVER

#include "angband.h"

/* Max radius of antimagic field generated by a monster */
#define MONSTER_ANTIDIS		8

/* Chance of weps/armors got discounted when enchanting via spells,
 * in percent.	[40] */
#define ENCHANT_DISCOUNT_CHANCE	40

/*
 * Rerturn the skill associated with the realm
 */
int find_realm_skill(int realm)
{
        switch (realm)
        {
        case REALM_MAGERY:
                return SKILL_MAGERY;
        case REALM_PRAYER:
                return SKILL_PRAY;
        case REALM_SORCERY:
                return SKILL_SORCERY;
        case REALM_SHADOW:
                return SKILL_SHADOW;
        case REALM_HUNT:
//                return SKILL_ARCHERY;
                return SKILL_HUNTING;
        case REALM_FIGHTING:
//                return SKILL_MASTERY;
                return SKILL_TECHNIC;
//        case REALM_PSI:
//                return SKILL_;
        };
        return 0;
}

/*
 * Returns spell chance of failure for spell		-RAK-
 */
static s16b spell_chance(int Ind, int realm, magic_type *s_ptr)
{
	player_type *p_ptr = Players[Ind];

	int		chance, minfail, minminfail;

	/* Extract the base spell failure rate */
	chance = s_ptr->sfail;

	/* Reduce failure rate by "effective" level adjustment */
	chance -= 3 * (get_skill(p_ptr, find_realm_skill(realm)) - s_ptr->slevel);

	/* Reduce failure rate by INT/WIS adjustment */
	//chance -= 3 * (adj_mag_stat[p_ptr->stat_ind[magic_info[realm].spell_stat]] - 1);

	/* Not enough mana to cast */
	if (s_ptr->smana > p_ptr->csp)
	{
		/* Hack -- Since at the moment casting spells without enough mana*/
		/* is impossible, I'm taking this out, as it confuses people. */
		/* chance += 5 * (s_ptr->smana - p_ptr->csp); */
	}

	/* Extract the minimum failure rate */
	//minfail = adj_mag_fail[p_ptr->stat_ind[magic_info[realm].spell_stat]];

	/* Non mage/sorceror/priest characters never get too good */
#if 0 // NEED to find a good way to do that without class
	if ((p_ptr->pclass != CLASS_MAGE) && (p_ptr->pclass != CLASS_PRIEST) && (p_ptr->pclass != CLASS_SORCERER))
	{
		if (minfail < 5) minfail = 5;
	}
#else
	minminfail = 8 - get_skill_scale(p_ptr, find_realm_skill(realm), 5)
		- get_skill_scale(p_ptr, SKILL_MAGIC, 3);
	if (minminfail > 5) minminfail = 5;

	if (minfail < minminfail) minfail = minminfail;

#endif
	/* Hack -- Priest prayer penalty for "edged" weapons  -DGK */
	if ((realm == REALM_PRAYER) && (p_ptr->icky_wield)) chance += 25;

	/* Minimum failure rate */
	if (chance < minfail) chance = minfail;

	/* Stunning makes spells harder */
	if (p_ptr->stun > 50) chance += 25;
	else if (p_ptr->stun) chance += 15;

	/* Always a 5 percent chance of working */
	if (chance > 95) chance = 95;

	/* Return the chance */
	return (chance);
}

/* ok, it's hacked :) */
/* of course, you can optimize it further by bandling
 * monsters and players into one loop..		- Jir - */
bool check_antimagic(int Ind)
{
	player_type *p_ptr = Players[Ind];
	worldpos *wpos = &p_ptr->wpos;
	monster_type *m_ptr;
	monster_race *r_ptr;
	cave_type **zcave;
	int i, x, y, x2 = p_ptr->px, y2 = p_ptr->py, m_idx;
		int dis, antichance, antidis;

	if (!(zcave=getcave(wpos))) return(FALSE);

	for (i = 1; i <= NumPlayers; i++)
	{
		player_type *q_ptr = Players[i];

		/* Skip disconnected players */
		if (q_ptr->conn == NOT_CONNECTED) continue;

		/* Skip players not on this depth */
		if (!inarea(&q_ptr->wpos, &p_ptr->wpos)) continue;

		/* Compute distance */
		if (q_ptr->antimagic_dis < distance(y2, x2, q_ptr->py, q_ptr->px))
			continue;

		antichance = q_ptr->antimagic;

		if (i != Ind) antichance -= p_ptr->lev >> 1;

		if (antichance > 95) antichance = 95;

		/* Reduction for party */
		if ((i != Ind) && player_in_party(p_ptr->party, i))
			antichance >>= 1;
#if 0
		dis = distance(y2, x2, q_ptr->py, q_ptr->px);
		antidis = q_ptr->antimagic_dis;
		if (dis > antidis) antichance = 0;
#endif	// 0

		/* Got disrupted ? */
		if (magik(antichance))
		{
			if (i == Ind) msg_print(Ind, "Your own anti-magic shield disrupts your attempts.");
			else msg_format(Ind, "%s's anti-magic shield disrupts your attempts.", q_ptr->name);
			return TRUE;
		}
	}

	/* Scan the maximal area of radius "MONSTER_ANTIDIS" */
	dis = 1;
	for (i = 1; i <= tdi[MONSTER_ANTIDIS]; i++)
	{
		if (i == tdi[dis]) dis++;

		y = y2 + tdy[i];
		x = x2 + tdx[i];

		/* Ignore "illegal" locations */
		if (!in_bounds2(wpos, y, x)) continue;

		if ((m_idx = zcave[y][x].m_idx)<=0) continue;

		m_ptr = &m_list[m_idx];	// pfft, bad design

		/* dont use removed monsters */
		if(!m_ptr->r_idx) continue;

		r_ptr = race_inf(m_ptr);

		if (!(r_ptr->flags7 & RF7_DISBELIEVE)) continue;

		antichance = r_ptr->level / 2 + 20;
		antidis = r_ptr->level / 15 + 3;

		if (dis > antidis) continue;
		if (antichance > 95) antichance = 95;

		/* Got disrupted ? */
		if (magik(antichance))
		{
			if (p_ptr->mon_vis[m_idx])
			{
				char m_name[80];
				monster_desc(Ind, m_name, m_idx, 0);
				msg_format(Ind, "%^s's anti-magic shield disrupts your attempts.", m_name);
			}
			else
			{
				msg_print(Ind, "An anti-magic shield disrupts your attempts.");
			}
			return TRUE;
		}
	}
#if 0
	/* Scan the maximal area of radius "MONSTER_ANTIDIS" */
	for (y = y2 - MONSTER_ANTIDIS; y <= y2 + MONSTER_ANTIDIS; y++)
	{
		for (x = x2 - MONSTER_ANTIDIS; x <= x2 + MONSTER_ANTIDIS; x++)
		{
			/* Ignore "illegal" locations */
			if (!in_bounds2(wpos, y, x)) continue;

			if ((m_idx = zcave[y][x].m_idx)<=0) continue;

			/* Enforce a "circular" explosion */
			if ((dis = distance(y2, x2, y, x)) > MONSTER_ANTIDIS) continue;

			m_ptr = &m_list[m_idx];	// pfft, bad design

			/* dont use removed monsters */
			if(!m_ptr->r_idx) continue;

			r_ptr = race_inf(m_ptr);

			if (!(r_ptr->flags7 & RF7_DISBELIEVE)) continue;

			antichance = r_ptr->level / 2 + 20;
			antidis = r_ptr->level / 15 + 3;

			if (dis > antidis) continue;
			if (antichance > 95) antichance = 95;

			/* Got disrupted ? */
			if (magik(antichance))
			{
				if (p_ptr->mon_vis[m_idx])
				{
					char m_name[80];
					monster_desc(Ind, m_name, m_idx, 0);
					msg_format(Ind, "%^s's anti-magic shield disrupts your attempts.", m_name);
				}
				else
				{
					msg_print(Ind, "An anti-magic shield disrupts your attempts.");
				}
				return TRUE;
			}
		}
	}
#endif	// 0
	/* Assume no antimagic */
	return FALSE;
}

/*
 * Brand the current weapon
 */
static void brand_weapon(int Ind)
{
	player_type *p_ptr = Players[Ind];

	object_type *o_ptr;

	o_ptr = &p_ptr->inventory[INVEN_WIELD];

	/* you can never modify artifacts / ego-items */
	/* you can never modify broken / cursed items */
	if ((o_ptr->k_idx) &&
	    (!artifact_p(o_ptr)) && (!ego_item_p(o_ptr)) &&
	    (!broken_p(o_ptr)) && (!cursed_p(o_ptr)))
	{
		cptr act = NULL;

		char o_name[160];

		if (rand_int(100) < 25)
		{
			act = "is covered in a fiery shield!";
			o_ptr->name2 = EGO_BRAND_FIRE;
		}

		else
		{
			act = "glows deep, icy blue!";
			o_ptr->name2 = EGO_BRAND_COLD;
		}

		object_desc(Ind, o_name, o_ptr, FALSE, 0);

		msg_format(Ind, "Your %s %s", o_name, act);

		enchant(Ind, o_ptr, rand_int(3) + 4, ENCH_TOHIT | ENCH_TODAM);

		/* Hack -- you don't sell the wep blessed by your god, do you? :) */
		o_ptr->discount = 100;
	}

	else
	{
		msg_print(Ind, "The Branding failed.");
	}
}

/*
 * Use a ghostly ability. --KLJ--
 */
void do_cmd_ghost_power(int Ind, int ability)
{
	player_type *p_ptr = Players[Ind];
	magic_type *s_ptr = &ghost_spells[ability];
	int plev = p_ptr->lev;
	int i, j = 0;

	/* Check for ghost-ness */
	if (!p_ptr->ghost) return;

	/* Must not be confused */
	if (p_ptr->confused)
	{
		/* Message */
		msg_print(Ind, "You are too confused!");
		return;
	}

	/* Check spells */
	for (i = 0; i < 64; i++)
	{
		s_ptr = &ghost_spells[i];

		/* Check for existance */
		if (s_ptr->slevel >= 99) continue;

		/* Next spell */
		if (j++ == ability) break;
	}

	/* Check for level */
	if (s_ptr->slevel > plev)
	{
		/* Message */
		msg_print(Ind, "You aren't powerful enough to use that ability.");
		return;
	}

	/* Spell effects */
	switch(i)
	{
		case 0:
		{
			teleport_player(Ind, 10);
			break;
		}
		case 1:
		{
			get_aim_dir(Ind);
			p_ptr->current_spell = 1;
			return;
		}
		case 2:
		{
			get_aim_dir(Ind);
			p_ptr->current_spell = 2;
			return;
		}
		case 3:
		{
			teleport_player(Ind, plev * 8);
			break;
		}
		case 4:
		{
			get_aim_dir(Ind);
			p_ptr->current_spell = 4;
			return;
		}
		case 5:
		{
			get_aim_dir(Ind);
			p_ptr->current_spell = 5;
			return;
		}
		case 6:
		{
			get_aim_dir(Ind);
			p_ptr->current_spell = 6;
			return;
		}
	}

	/* Take a turn */
	p_ptr->energy -= level_speed(&p_ptr->wpos);

	take_xp_hit(Ind, s_ptr->slevel * s_ptr->smana,
			"the strain of ghostly powers", TRUE, TRUE);
#if 0
	/* Take some experience */
	p_ptr->max_exp -= s_ptr->slevel * s_ptr->smana;
	p_ptr->exp -= s_ptr->slevel * s_ptr->smana;

	/* Too much can kill you */
	if (p_ptr->exp < 0) take_hit(Ind, 5000, "the strain of ghostly powers");

	/* Check experience levels */
	check_experience(Ind);

	/* Redraw experience */
	p_ptr->redraw |= (PR_EXP);

	/* Window stuff */
	p_ptr->window |= (PW_PLAYER);
#endif	// 0
}


/*
 * Directional ghost ability
 */
void do_cmd_ghost_power_aux(int Ind, int dir)
{
	player_type *p_ptr = Players[Ind];
	int plev = p_ptr->lev;
	magic_type *s_ptr;
	
	/* Verify spell number */
	if (p_ptr->current_spell < 0)
		return;

	/* Acquire spell pointer */
	s_ptr = &ghost_spells[p_ptr->current_spell];

	/* We assume everything is still OK to cast */
	switch (p_ptr->current_spell)
	{
		case 1:
		{
			(void)fear_monster(Ind, dir, plev);
			break;
		}
		case 2:
		{
			confuse_monster(Ind, dir, plev);
			break;
		}
		case 4:
		{
			fire_bolt_or_beam(Ind, plev * 2, GF_NETHER, dir, 50 + damroll(5, 5) + plev);
			break;
		}
		case 5:
		{
			fire_ball(Ind, GF_NETHER, dir, 100 + 2 * plev, 2);
			break;
		}
		case 6:
		{
			fire_ball(Ind, GF_DARK, dir, plev * 5 + damroll(10, 10), 3);
			break;
		}
	}

	/* No more spell */
	p_ptr->current_spell = -1;

	/* Take a turn */
	p_ptr->energy -= level_speed(&p_ptr->wpos);

	take_xp_hit(Ind, s_ptr->slevel * s_ptr->smana,
			"the strain of ghostly powers", TRUE, TRUE);
#if 0
	/* Take some experience */
	p_ptr->max_exp -= s_ptr->slevel * s_ptr->smana;
	p_ptr->exp -= s_ptr->slevel * s_ptr->smana;

	/* Too much can kill you */
	if (p_ptr->exp < 0) take_hit(Ind, 5000, "the strain of ghostly powers");

	/* Check experience levels */
	check_experience(Ind);

	/* Redraw experience */
	p_ptr->redraw |= (PR_EXP);

	/* Window stuff */
	p_ptr->window |= (PW_PLAYER);
#endif	// 0
}

void do_spin(int Ind)
{
	player_type *p_ptr = Players[Ind];
	int d;

	for (d = 1; d <= 9; d++)
	{
		int x, y;

		if (d == 5) continue;

		x = p_ptr->px + ddx[d];
		y = p_ptr->py + ddy[d];

		if (!in_bounds(y, x)) continue;
		py_attack(Ind, y, x, TRUE);
	}
}

static void do_mimic_power(int Ind, int power)
{
	player_type *p_ptr = Players[Ind];
	monster_race *r_ptr = &r_info[p_ptr->body_monster];
	int rlev = r_ptr->level;
	int j, k, chance;
	magic_type *s_ptr = &innate_powers[power];

//	j = power;


	/* Not when confused */
	if (p_ptr->confused)
	{
		msg_print(Ind, "You are too confused!");
		return;
	}

	j = power / 32;

	if (j < 0 || j > 3)
	{
		msg_format(Ind, "SERVER ERROR: Tried to use a strange innate power(%d)!", power);
		return;
	}

	/* confirm the power */
	if (!p_ptr->innate_spells[j] & (1L << (power - j * 3))) 
	{
		msg_print(Ind, "You cannot use that power.");
		return;
	}

	j = power;

	/* Check mana */
	if (s_ptr->smana > p_ptr->csp)
	{
		msg_print(Ind, "You do not have enough mana.");
		return;
	}

	p_ptr->energy -= level_speed(&p_ptr->wpos);

	/* Spell failure chance -- Hack, use the same stats as magery*/
//	chance = spell_chance(Ind, REALM_MAGERY, s_ptr);
	chance = spell_chance(Ind, REALM_MIMIC, s_ptr);

	if (j >= 32 && interfere(Ind, cfg.spell_interfere)) return;

	/* Failed spell */
	if (rand_int(100) < chance)
	{
		msg_print(Ind, "You failed to use the power!");
	}
	else
	{
		/* Hack -- preserve current 'realm' */
		p_ptr->current_realm = REALM_MIMIC;


  /* 0-31 = RF4, 32-63 = RF5, 64-95 = RF6 */
  switch(j)
    {
    case 0:
      msg_print(Ind, "You emit a high pitched humming noise.");
      aggravate_monsters(Ind, 1);
      break;
    case 1:
      break;
    case 2:
      break;
    case 3:
      break;
//#define RF4_ARROW_1			0x00000010	/* Fire an arrow (light) */
    case 4:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
//#define RF4_ARROW_2			0x00000020	/* Fire an arrow (heavy) */
    case 5:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
//#define RF4_ARROW_3			0x00000040	/* Fire missiles (light) */
    case 6:
      break;
//#define RF4_ARROW_4			0x00000080	/* Fire missiles (heavy) */
    case 7:
      break;
//#define RF4_BR_ACID			0x00000100	/* Breathe Acid */
    case 8:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
//#define RF4_BR_ELEC			0x00000200	/* Breathe Elec */
    case 9:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
//#define RF4_BR_FIRE			0x00000400	/* Breathe Fire */
    case 10:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
//#define RF4_BR_COLD			0x00000800	/* Breathe Cold */
    case 11:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
//#define RF4_BR_POIS			0x00001000	/* Breathe Poison */
    case 12:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
//#define RF4_BR_NETH			0x00002000	/* Breathe Nether */
    case 13:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_LITE			0x00004000	/* Breathe Lite */
    case 14:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_DARK			0x00008000	/* Breathe Dark */
    case 15:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_CONF			0x00010000	/* Breathe Confusion */
    case 16:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_SOUN			0x00020000	/* Breathe Sound */
    case 17:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_CHAO			0x00040000	/* Breathe Chaos */
    case 18:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_DISE			0x00080000	/* Breathe Disenchant */
    case 19:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_NEXU			0x00100000	/* Breathe Nexus */
    case 20:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_TIME			0x00200000	/* Breathe Time */
    case 21:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_INER			0x00400000	/* Breathe Inertia */
    case 22:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_GRAV			0x00800000	/* Breathe Gravity */
    case 23:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_SHAR			0x01000000	/* Breathe Shards */
    case 24:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_PLAS			0x02000000	/* Breathe Plasma */
    case 25:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_WALL			0x04000000	/* Breathe Force */
    case 26:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_BR_MANA			0x08000000	/* Breathe Mana */
    case 27:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_XXX5			0x10000000
    case 28:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_XXX6			0x20000000
    case 29:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
//#define RF4_XXX7			0x40000000
    case 30:
      break;
// #define RF4_BOULDER			0x80000000
    case 31:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;

/* RF5 */

// RF5_BA_ACID			0x00000001	/* Acid Ball */
    case 32:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BA_ELEC			0x00000002	/* Elec Ball */
    case 33:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BA_FIRE			0x00000004	/* Fire Ball */
    case 34:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BA_COLD			0x00000008	/* Cold Ball */
    case 35:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BA_POIS			0x00000010	/* Poison Ball */
    case 36:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BA_NETH			0x00000020	/* Nether Ball */
    case 37:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BA_WATE			0x00000040	/* Water Ball */
    case 38:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BA_MANA			0x00000080	/* Mana Storm */
    case 39:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BA_DARK			0x00000100	/* Darkness Storm */
    case 40:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_DRAIN_MANA		0x00000200	/* Drain Mana */
    case 41:
      msg_print(Ind, "Haha, you wish ... :)");
      break;
// RF5_MIND_BLAST		0x00000400	/* Blast Mind */
    case 42:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BRAIN_SMASH		0x00000800	/* Smash Brain */
    case 43:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_CAUSE_1			0x00001000	/* Cause Light Wound */
    case 44:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_CAUSE_2			0x00002000	/* Cause Serious Wound */
    case 45:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BA_NUKE			0x00004000	/* Cause Critical Wound */
    case 46:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BA_CHAO			0x00008000	/* Cause Mortal Wound */
    case 47:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BO_ACID			0x00010000	/* Acid Bolt */
    case 48:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BO_ELEC			0x00020000	/* Elec Bolt (unused) */
    case 49:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BO_FIRE			0x00040000	/* Fire Bolt */
    case 50:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BO_COLD			0x00080000	/* Cold Bolt */
    case 51:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BO_POIS			0x00100000	/* Poison Bolt (unused) */
    case 52:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BO_NETH			0x00200000	/* Nether Bolt */
    case 53:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BO_WATE			0x00400000	/* Water Bolt */
    case 54:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BO_MANA			0x00800000	/* Mana Bolt */
    case 55:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BO_PLAS			0x01000000	/* Plasma Bolt */
    case 56:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BO_ICEE			0x02000000	/* Ice Bolt */
    case 57:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_MISSILE			0x04000000	/* Magic Missile */
    case 58:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_SCARE			0x08000000	/* Frighten Player */
    case 59:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_BLIND			0x10000000	/* Blind Player */
    case 60:
      msg_print(Ind, "Haha, you wish ... :)");
      break;
// RF5_CONF			0x20000000	/* Confuse Player */
    case 61:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_SLOW			0x40000000	/* Slow Player */
    case 62:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF5_HOLD			0x80000000	/* Paralyze Player */
    case 63:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;

// RF6_HASTE			0x00000001	/* Speed self */
    case 64:
      if(!p_ptr->fast) set_fast(Ind, 10 + (rlev / 2));
      break;
// RF6_HAND_DOOM		0x00000002	/* Should we...? */ /* YES! */
    case 65:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF6_HEAL			0x00000004	/* Heal self */
    case 66:
      hp_player(Ind, rlev * 2);
      break;
// RF6_XXX2			0x00000008	/* Heal a lot (?) */
    case 67:
      break;
// RF6_BLINK			0x00000010	/* Teleport Short */
    case 68:
      teleport_player(Ind, 10);
      break;
// RF6_TPORT			0x00000020	/* Teleport Long */
    case 69:
      teleport_player(Ind, 200);
      break;
// RF6_XXX3			0x00000040	/* Move to Player (?) */
    case 70:
      break;
// RF6_XXX4			0x00000080	/* Move to Monster (?) */
    case 71:
      break;
// RF6_TELE_TO			0x00000100	/* Move player to monster */
    case 72:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF6_TELE_AWAY		0x00000200	/* Move player far away */
    case 73:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF6_TELE_LEVEL		0x00000400	/* Move player vertically */
    case 74:
      teleport_player_level(Ind);	/* wrong way, but useful */
      break;
// RF6_XXX5			0x00000800	/* Move player (?) */
    case 75:
      break;
// RF6_DARKNESS		0x00001000	/* Create Darkness */
    case 76:
	  unlite_area(Ind, 10, 3);
      break;
// RF6_TRAPS			0x00002000	/* Create Traps */
    case 77:
      get_aim_dir(Ind);
      p_ptr->current_spell = j;
      return;
      break;
// RF6_FORGET			0x00004000	/* Cause amnesia */
    case 78:
      msg_print(Ind, "Haha, you wish ... :)");
      break;

    default:
      msg_format(Ind, "Bad innate power %d.", power);
      break;
    }
	}

	if (s_ptr->smana <= p_ptr->csp)
	{
		/* Use some mana */
		p_ptr->csp -= s_ptr->smana;
	}

	/* Over-exert the player */
	else
	{
		int oops = s_ptr->smana - p_ptr->csp;

		/* No mana left */
		p_ptr->csp = 0;
		p_ptr->csp_frac = 0;

		/* Message */
		msg_print(Ind, "You faint from the effort!");

		/* Hack -- bypass free action */
		(void)set_paralyzed(Ind, p_ptr->paralyzed + randint(5 * oops + 1));

		/* Damage CON (possibly permanently) */
		if (rand_int(100) < 50)
		{
			bool perm = (rand_int(100) < 25);

			/* Message */
			msg_print(Ind, "You have damaged your health!");

			/* Reduce constitution */
			(void)dec_stat(Ind, A_CON, 15 + randint(10), perm);
		}
	}
}

/*
 * Finish casting a spell that required a direction --KLJ--
 */
void do_mimic_power_aux(int Ind, int dir)
{
	player_type *p_ptr = Players[Ind];
	monster_race *r_ptr = &r_info[p_ptr->body_monster];
	int rad;
	int rlev = r_ptr->level;
	magic_type *s_ptr = &innate_powers[p_ptr->current_spell];

	/* Determine the radius of the blast */
	rad = (r_ptr->flags2 & RF2_POWERFUL) ? 3 : 2;

	/* Only fire in direction 5 if we have a target */
	if ((dir == 5) && !target_okay(Ind))
	{
		/* Reset current spell */
		p_ptr->current_spell = -1;

		/* Done */
		return;
	}

	/* We assume that the spell can be cast, and so forth */
	switch(p_ptr->current_spell)
	{
//#define RF4_ARROW_1			0x00000010	/* Fire arrow(s) */
		/* XXX: ARROW_1 gives extra-shot to the player; we'd better
		 * remove this 'innate' power? (see calc_body_bonus) */
    case 4:
	{
		int k;
		for (k = 0; k < 1 + rlev / 20; k++)
		{
			fire_bolt(Ind, GF_ARROW, dir, damroll(1 + rlev / 8, 6));
			break;
		}
	}
//#define RF4_ARROW_2			0x00000020	/* Fire missiles */
    case 5:
      fire_bolt(Ind, GF_ARROW, dir, damroll(3 + rlev / 15, 6));
      break;
//#define RF4_ARROW_3			0x00000040	/* XXX */
    case 6:
      break;
//#define RF4_ARROW_4			0x00000080	/* XXX */
    case 7:
      break;
//#define RF4_BR_ACID			0x00000100	/* Breathe Acid */
    case 8:
      fire_ball(Ind, GF_ACID, dir, ((p_ptr->chp / 2) > 500) ? 500 : (p_ptr->chp / 2), rad);
      break;
//#define RF4_BR_ELEC			0x00000200	/* Breathe Elec */
    case 9:
      fire_ball(Ind, GF_ELEC, dir, ((p_ptr->chp / 2) > 500) ? 500 : (p_ptr->chp / 2) , rad);
      break;
//#define RF4_BR_FIRE			0x00000400	/* Breathe Fire */
    case 10:
      fire_ball(Ind, GF_FIRE, dir, ((p_ptr->chp / 2) > 500) ? 500 : (p_ptr->chp / 2), rad );
      break;
//#define RF4_BR_COLD			0x00000800	/* Breathe Cold */
    case 11:
      fire_ball(Ind, GF_COLD, dir, ((p_ptr->chp / 2) > 500) ? 500 : (p_ptr->chp / 2), rad );
      break;
//#define RF4_BR_POIS			0x00001000	/* Breathe Poison */
    case 12:
      fire_ball(Ind, GF_POIS, dir, ((p_ptr->chp / 2) > 500) ? 500 : (p_ptr->chp / 2), rad );
      break;
//#define RF4_BR_NETH			0x00002000	/* Breathe Nether */
    case 13:
      fire_ball(Ind, GF_NETHER, dir, ((p_ptr->chp / 3) > 500) ? 500 : (p_ptr->chp / 3), rad );
      break;
//#define RF4_BR_LITE			0x00004000	/* Breathe Lite */
    case 14:
      fire_ball(Ind, GF_LITE, dir, ((p_ptr->chp / 3) > 400) ? 400 : (p_ptr->chp / 3), rad );
      break;
//#define RF4_BR_DARK			0x00008000	/* Breathe Dark */
    case 15:
      fire_ball(Ind, GF_DARK, dir, ((p_ptr->chp / 3) > 400) ? 400 : (p_ptr->chp / 3) , rad);
      break;
//#define RF4_BR_CONF			0x00010000	/* Breathe Confusion */
    case 16:
      fire_ball(Ind, GF_CONFUSION, dir, ((p_ptr->chp / 3) > 400) ? 400 : (p_ptr->chp / 3), rad );
      break;
//#define RF4_BR_SOUN			0x00020000	/* Breathe Sound */
    case 17:
      fire_ball(Ind, GF_SOUND, dir, ((p_ptr->chp / 4) > 350) ? 350 : (p_ptr->chp / 4), rad );
      break;
//#define RF4_BR_CHAO			0x00040000	/* Breathe Chaos */
    case 18:
      fire_ball(Ind, GF_CHAOS, dir, ((p_ptr->chp / 4) > 500) ? 500 : (p_ptr->chp / 4), rad );
      break;
//#define RF4_BR_DISE			0x00080000	/* Breathe Disenchant */
    case 19:
      fire_ball(Ind, GF_DISENCHANT, dir, ((p_ptr->chp / 4) > 400) ? 400 : (p_ptr->chp / 4), rad );
      break;
//#define RF4_BR_NEXU			0x00100000	/* Breathe Nexus */
    case 20:
      fire_ball(Ind, GF_NEXUS, dir, ((p_ptr->chp / 3) > 250) ? 250 : (p_ptr->chp / 3), rad );
      break;
//#define RF4_BR_TIME			0x00200000	/* Breathe Time */
    case 21:
      fire_ball(Ind, GF_TIME, dir, ((p_ptr->chp / 4) > 200) ? 200 : (p_ptr->chp / 4), rad );
      break;
//#define RF4_BR_INER			0x00400000	/* Breathe Inertia */
    case 22:
      fire_ball(Ind, GF_INERTIA, dir, ((p_ptr->chp / 4) > 200) ? 200 : (p_ptr->chp / 4) , rad);
      break;
//#define RF4_BR_GRAV			0x00800000	/* Breathe Gravity */
    case 23:
      fire_ball(Ind, GF_GRAVITY, dir, ((p_ptr->chp / 3) > 200) ? 200 : (p_ptr->chp / 3) , rad);
      break;
//#define RF4_BR_SHAR			0x01000000	/* Breathe Shards */
    case 24:
      fire_ball(Ind, GF_SHARDS, dir, ((p_ptr->chp / 4) > 300) ? 300 : (p_ptr->chp / 4) , rad);
      break;
//#define RF4_BR_PLAS			0x02000000	/* Breathe Plasma */
    case 25:
      fire_ball(Ind, GF_PLASMA, dir, ((p_ptr->chp / 4) > 150) ? 150 : (p_ptr->chp / 4) , rad);
      break;
//#define RF4_BR_WALL			0x04000000	/* Breathe Force */
    case 26:
      fire_ball(Ind, GF_FORCE, dir, ((p_ptr->chp / 4) > 200) ? 200 : (p_ptr->chp / 4) , rad);
      break;
//#define RF4_BR_MANA			0x08000000	/* Breathe Mana */
    case 27:
      fire_ball(Ind, GF_MANA, dir, ((p_ptr->chp / 3) > 500) ? 500 : (p_ptr->chp / 3) , rad);
      break;
	/* RF4_BR_DISI */
	case 28:
	  fire_ball(Ind, GF_DISINTEGRATE, dir,
			  ((p_ptr->chp / 3) > 300 ? 300 : (p_ptr->chp / 3)), rad);
	  break;
	/* RF4_BR_NUKE */
	case 29:
	  fire_ball(Ind, GF_NUKE, dir,
			  ((p_ptr->chp / 3) > 800 ? 800 : (p_ptr->chp / 3)), rad);
	  break;
	/* RF4_BOULDER */
    case 31:
      fire_bolt(Ind, GF_ARROW, dir, damroll(1 + rlev / 7, 12));
      break;
		 

/* RF5 */

// RF5_BA_ACID			0x00000001	/* Acid Ball */
    case 32:
      fire_ball(Ind, GF_ACID, dir, randint(rlev * 3) + 15 , rad);
      break;
// RF5_BA_ELEC			0x00000002	/* Elec Ball */
    case 33:
      fire_ball(Ind, GF_ELEC, dir, randint(rlev * 3 / 2) + 8 , rad);
      break;
// RF5_BA_FIRE			0x00000004	/* Fire Ball */
    case 34:
      fire_ball(Ind, GF_FIRE, dir, randint(rlev * 7 / 0) + 10 , rad);
      break;
// RF5_BA_COLD			0x00000008	/* Cold Ball */
    case 35:
      fire_ball(Ind, GF_COLD, dir, randint(rlev * 3 / 2) + 10 , rad);
      break;
// RF5_BA_POIS			0x00000010	/* Poison Ball */
    case 36:
      fire_ball(Ind, GF_POIS, dir, damroll(12, 2) , rad);
      break;
// RF5_BA_NETH			0x00000020	/* Nether Ball */
    case 37:
      fire_ball(Ind, GF_NETHER, dir, 50  + damroll(10, 10) + rlev , rad);
      break;
// RF5_BA_WATE			0x00000040	/* Water Ball */
    case 38:
      fire_ball(Ind, GF_WATER, dir, randint(rlev * 5 / 2) + 50 , rad);
      break;
// RF5_BA_MANA			0x00000080	/* Mana Storm */
    case 39:
      fire_ball(Ind, GF_MANA, dir, damroll(10, 10) + (rlev * 2) , rad);
      break;
// RF5_BA_DARK			0x00000100	/* Darkness Storm */
    case 40:
      fire_ball(Ind, GF_DARK, dir, damroll(10, 10) + (rlev * 2) , rad);
      break;
// RF5_MIND_BLAST		0x00000400	/* Blast Mind */
    case 42:
      fire_bolt(Ind, GF_PSI, dir, damroll(3 + rlev / 5, 8));
      break;
// RF5_BRAIN_SMASH		0x00000800	/* Smash Brain */
    case 43:
      fire_bolt(Ind, GF_PSI, dir, damroll(5 + rlev / 4, 8));
      break;
// RF5_CAUSE_1			0x00001000	/* Cause Wound */
    case 44:
      fire_bolt(Ind, GF_MANA, dir, damroll(3 + rlev / 4, 8));
      break;
// RF5_CAUSE_2			0x00002000	/* XXX */
    case 45:
      break;
	/* RF5_BA_NUKE */
	case 32+14:
	  fire_ball(Ind, GF_NUKE, dir, (rlev + damroll(10, 6)), 2);
	  break;
	/* RF5_BA_CHAO */
	case 32+15:
	  fire_ball(Ind, GF_CHAOS, dir, (rlev * 2) + damroll(10, 10), 4);
	  break;
// RF5_BO_ACID			0x00010000	/* Acid Bolt */
    case 48:
      fire_bolt(Ind, GF_ACID, dir, damroll(7, 8) + (rlev / 3));
      break;
// RF5_BO_ELEC			0x00020000	/* Elec Bolt (unused) */
    case 49:
      fire_bolt(Ind, GF_ELEC, dir, damroll(4, 8) + (rlev / 3));
      break;
// RF5_BO_FIRE			0x00040000	/* Fire Bolt */
    case 50:
      fire_bolt(Ind, GF_FIRE, dir, damroll(9, 8) + (rlev / 3));
      break;
// RF5_BO_COLD			0x00080000	/* Cold Bolt */
    case 51:
      fire_bolt(Ind, GF_COLD, dir, damroll(6, 8) + (rlev / 3));
      break;
// RF5_BO_POIS			0x00100000	/* Poison Bolt (unused) */
    case 52:
      fire_bolt(Ind, GF_POIS, dir, damroll(3, 8) + (rlev / 3));
      break;
// RF5_BO_NETH			0x00200000	/* Nether Bolt */
    case 53:
      fire_bolt(Ind, GF_NETHER, dir, 30 + damroll(5, 5) + (rlev * 3) / 2);
      break;
// RF5_BO_WATE			0x00400000	/* Water Bolt */
    case 54:
      fire_bolt(Ind, GF_WATER, dir, damroll(10, 10) + (rlev));
      break;
// RF5_BO_MANA			0x00800000	/* Mana Bolt */
    case 55:
      fire_bolt(Ind, GF_MANA, dir, randint(rlev * 7 / 4) + 50);
      break;
// RF5_BO_PLAS			0x01000000	/* Plasma Bolt */
    case 56:
      fire_bolt(Ind, GF_PLASMA, dir, 10 + damroll(8, 7) + (rlev));
      break;
// RF5_BO_ICEE			0x02000000	/* Ice Bolt */
    case 57:
      fire_bolt(Ind, GF_ICE, dir, damroll(6, 6) + (rlev));
      break;
// RF5_MISSILE			0x04000000	/* Magic Missile */
    case 58:
      fire_bolt(Ind, GF_MISSILE, dir, damroll(2, 6) + (rlev / 3));
      break;
// RF5_SCARE			0x08000000	/* Frighten Player */
    case 59:
      fire_bolt(Ind, GF_TURN_ALL, dir, damroll(2, 6) + (rlev / 3));
      break;
// RF5_CONF			0x20000000	/* Confuse Player */
    case 61:
      fire_bolt(Ind, GF_CONFUSION, dir, damroll(2, 6) + (rlev / 3));
      break;
// RF5_SLOW			0x40000000	/* Slow Player */
    case 62:
      fire_bolt(Ind, GF_OLD_SLOW, dir, damroll(2, 6) + (rlev / 3));
      break;
// RF5_HOLD			0x80000000	/* Paralyze Player */
    case 63:
      fire_bolt(Ind, GF_STASIS, dir, damroll(2, 6) + (rlev / 3));
// RF6_HAND_DOOM		0x00000002	/* Should we...? */ /* YES! */
    case 65:
	  (void)project_hook(Ind, GF_HAND_DOOM, dir, 1, PROJECT_STOP | PROJECT_KILL);
      break;
// RF6_TELE_TO
	case 72:
	  (void)project_hook(Ind, GF_TELE_TO, dir, 1, PROJECT_STOP | PROJECT_KILL);
	  break;
// RF6_TELE_AWAY
	case 73:
          (void)fire_beam(Ind, GF_AWAY_ALL, dir, rlev);
	  break;
// RF6_TRAPS			0x00002000	/* Create Traps */
    case 77:
	  /* I dunno if you're happy with it :) */
      fire_ball(Ind, GF_MAKE_TRAP, dir, 1, 1 + rlev / 30);
      break;

		default:  /* For some reason we got called for a spell that */
		{         /* doesn't require a direction */
			msg_format(Ind, "SERVER ERROR: do_mimic_power_aux() called for non-directional power %d!", p_ptr->current_spell);
			p_ptr->current_spell = -1;
			return;
		}
	}	

//	p_ptr->energy -= level_speed(&p_ptr->wpos);

	if (s_ptr->smana <= p_ptr->csp)
	{
		/* Use some mana */
		p_ptr->csp -= s_ptr->smana;
	}

	/* Over-exert the player */
	else
	{
		int oops = s_ptr->smana - p_ptr->csp;

		/* No mana left */
		p_ptr->csp = 0;
		p_ptr->csp_frac = 0;

		/* Message */
		msg_print(Ind, "You faint from the effort!");

		/* Hack -- bypass free action */
		(void)set_paralyzed(Ind, p_ptr->paralyzed + randint(5 * oops + 1));

		/* Damage CON (possibly permanently) */
		if (rand_int(100) < 50)
		{
			bool perm = (rand_int(100) < 25);

			/* Message */
			msg_print(Ind, "You have damaged your health!");

			/* Reduce constitution */
			(void)dec_stat(Ind, A_CON, 15 + randint(10), perm);
		}
	}

	/* Reset current spell */
	p_ptr->current_spell = -1;

	/* Resend mana */
	p_ptr->redraw |= (PR_MANA);

	/* Window stuff */
	p_ptr->window |= (PW_PLAYER);
}

void do_mimic_change(int Ind, int r_idx)
{
	player_type *p_ptr = Players[Ind];

	if (r_info[r_idx].level > get_skill_scale(p_ptr, SKILL_MIMIC, 100))
	{
		msg_print(Ind, "You do need a higher mimicry skill to use that shape.");
		return;
	}

	p_ptr->body_monster = r_idx;
	p_ptr->body_changed = TRUE;
	
	msg_format(Ind, "You polymorph into a %s !", r_info[r_idx].name + r_name);
	msg_format_near(Ind, "%s polymorphs a % !", p_ptr->name, r_info[r_idx].name + r_name);

	note_spot(Ind, p_ptr->py, p_ptr->px);
	everyone_lite_spot(&p_ptr->wpos, p_ptr->py, p_ptr->px);

	/* Recalculate mana */
	p_ptr->update |= (PU_MANA | PU_HP | PU_BONUS | PU_VIEW);

	/* Tell the client */
	p_ptr->redraw |= PR_VARIOUS;

	/* Window stuff */
	p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER);
}

void do_cmd_mimic(int Ind, int spell)
{
	player_type *p_ptr = Players[Ind];
	int j, k;

	/* should it..? */
	dun_level		*l_ptr = getfloor(&p_ptr->wpos);
	if(l_ptr && l_ptr->flags1 & LF1_NO_MAGIC) return;

	if (!get_skill(p_ptr, SKILL_MIMIC))
	{
		msg_print(Ind, "You are too solid.");
		return;
	}

	/* No anti-magic shields around ? */
	/* Innate powers aren't hindered */
	if ((!spell || spell - 1 >= 32) && check_antimagic(Ind)) {
		p_ptr->energy -= level_speed(&p_ptr->wpos);
		return;
	}
	if (spell == 0){
		j = p_ptr->body_monster;
		k = 0;
		while (TRUE){
			j++;
			k++;
			if (k > MAX_R_IDX)
			{
//				j = 0;
				msg_print(Ind, "You don't know any forms!");
				return;
			}

			if (j >= MAX_R_IDX - 1) j = 0;

			if (r_info[j].level > get_skill_scale(p_ptr, SKILL_MIMIC, 100)) continue;
			if (r_info[j].flags1 & RF1_UNIQUE) continue;
			if (p_ptr->r_killed[j] < r_info[j].level) continue;
			if (p_ptr->r_killed[j] < 1 && j) continue;
			if (strlen(r_info[j].name + r_name) <= 1) continue;
			if (!r_info[j].level && !mon_allowed(&r_info[j])) continue;

			/* Ok we found */
			break;
		}
		do_mimic_change(Ind, j);
		p_ptr->energy -= level_speed(&p_ptr->wpos);
	}
	else {
		do_mimic_power(Ind, spell - 1);
	}
}

/*
 * School spells !
 */
void cast_school_spell(int Ind, int spell, int dir, int item)
{
        player_type *p_ptr = Players[Ind];

	/* No magic */
	if (p_ptr->antimagic)
	{
		msg_print(Ind, "Your anti-magic field disrupts any magic attempts.");
		return;
	}

	/* Actualy cast the choice */
	if (spell != -1)
        {
                exec_lua(Ind, format("cast_school_spell(%d, %d, spell(%d), nil, {dir = %d, item = %d})", Ind, spell, spell, dir, item));
	}
}
