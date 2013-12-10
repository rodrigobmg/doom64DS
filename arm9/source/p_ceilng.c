#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_spec.h"
#include "s_sound.h"
#include "sounds.h"
#include "r_local.h"
#include "d_main.h"

//
// CEILINGS
//


ceiling_t*	activeceilings[MAXCEILINGS];


//
// T_MoveCeiling
//

void T_MoveCeiling (ceiling_t* ceiling)
{
	result_e	res;

	switch(ceiling->direction)
	{
		case 0:
			// IN STASIS
			break;
		case 1:
			// UP
			res = T_MovePlane(ceiling->sector, ceiling->speed,
				ceiling->topheight, false,1,ceiling->direction);

			if(!ceiling->instant)
			{
				if(!(leveltime & 7))
					S_StartSound((mobj_t *)&ceiling->sector->soundorg, sfx_secmove);
			}

			if(res == pastdest)
			{
				switch(ceiling->type)
				{
					case lowerToFloor:
					case customCeiling:
					case crushAndRaiseOnce:
					case customCeilingToHeight:
						P_RemoveActiveCeiling(ceiling);
						break;
					case lowerAndCrush:
					case crushAndRaise:
                    case silentCrushAndRaise:
					case fastCrushAndRaise:
						ceiling->direction = -1;
						break;
					default:
						break;
				}
			}
			break;
		case -1:
			// DOWN
			res = T_MovePlane(ceiling->sector, ceiling->speed,
				ceiling->bottomheight, ceiling->crush, 1, ceiling->direction);

			if(!ceiling->instant)
			{
				if(!(leveltime & 7))
					S_StartSound((mobj_t *)&ceiling->sector->soundorg, sfx_secmove);
			}

			if(res == pastdest)
			{
				switch(ceiling->type)
				{
					case crushAndRaise:
					case silentCrushAndRaise:
						ceiling->speed = CEILSPEED;
					case fastCrushAndRaise:
					case crushAndRaiseOnce:
						ceiling->direction = 1;
						break;
					case lowerToFloor:
					case lowerAndCrush:
					case customCeiling:
					case customCeilingToHeight:
						P_RemoveActiveCeiling(ceiling);
						break;
					default:
						break;
				}
			}
			else
			{
				if(res == crushed)
				{
					if(ceiling->type == lowerAndCrush || ceiling->type == crushAndRaise)
						ceiling->speed = CEILSPEED / 8;
				}
			}
			break;
	}
}


//
// EV_DoCeiling
// Move a ceiling up/down and all around!
//

int EV_DoCeiling(line_t* line, ceiling_e type, fixed_t speed)
{
	int		secnum;
	int		rtn;
	sector_t*	sec;
	ceiling_t*	ceiling;

	secnum = -1;
	rtn = 0;

	// Reactivate in-stasis ceilings...for certain types.
	switch(type)
	{
		case crushAndRaise:
		case fastCrushAndRaise:
		case silentCrushAndRaise:
			P_ActivateInStasisCeiling(line);
		default:
			break;
	}

	while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
	{
		sec = &sectors[secnum];
		if (sec->specialdata)
			continue;

		// new door thinker
		rtn = 1;
		ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVSPEC, 0);
		P_AddThinker (&ceiling->thinker);
		sec->specialdata = ceiling;
		// Midway assumed that ceiling->instant is true only if the
		// speed is equal to 2048*FRACUNIT, which doesn't seem very sufficient
		ceiling->instant = (speed >= (CEILSPEED * 1024));
		ceiling->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
		ceiling->sector = sec;
		ceiling->crush = false;

		switch(type)
		{
			case silentCrushAndRaise:
				ceiling->instant = true;
			case fastCrushAndRaise:
			case crushAndRaiseOnce:
				ceiling->crush = true;
				ceiling->topheight = sec->ceilingheight;
				ceiling->bottomheight = sec->floorheight + (8*FRACUNIT);
				ceiling->direction = -1;
				ceiling->speed = speed;
				break;

			case crushAndRaise:
				ceiling->crush = true;
				ceiling->topheight = sec->ceilingheight;
			case lowerAndCrush:
			case lowerToFloor:
				ceiling->bottomheight = sec->floorheight;
				if(type != lowerToFloor)
					ceiling->bottomheight += 8*FRACUNIT;
				ceiling->direction = -1;
				ceiling->speed = speed;
				break;

			case raiseToHighest:
				ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
				ceiling->direction = 1;
				ceiling->speed = speed;
				break;

			case customCeiling:
				ceiling->speed = speed;
				if(globalint >= 0)
				{
					ceiling->direction = 1;
					ceiling->topheight = ceiling->sector->ceilingheight + (globalint * FRACUNIT);
				}
				else
				{
					ceiling->direction = -1;
					ceiling->bottomheight = ceiling->sector->ceilingheight + (globalint * FRACUNIT);
				}
				break;

			case customCeilingToHeight:
				ceiling->speed = speed;
				if((globalint * FRACUNIT) < ceiling->sector->ceilingheight)
				{
					ceiling->bottomheight = (globalint * FRACUNIT);
					ceiling->direction = -1;
				}
				else
				{
					ceiling->topheight = (globalint * FRACUNIT);
					ceiling->direction = 1;
				}
				break;

		}

		ceiling->tag = sec->tag;
		ceiling->type = type;
		P_AddActiveCeiling(ceiling);
	}
	return rtn;
}


//
// P_AddActiveCeiling
// Add an active ceiling
//

void P_AddActiveCeiling(ceiling_t* c)
{
	int	i;

	for(i = 0; i < MAXCEILINGS; i++)
    {
		if(activeceilings[i] == NULL)
		{
			activeceilings[i] = c;
			return;
		}
	}

    // [d64] added error message
    I_Error("P_AddActiveCeiling: no more ceiling slots");
}



//
// P_RemoveActiveCeiling
// Remove a ceiling's thinker
//

void P_RemoveActiveCeiling(ceiling_t* c)
{
    int	i;

	for(i = 0; i < MAXCEILINGS; i++)
	{
		if(activeceilings[i] == c)
		{
			activeceilings[i]->sector->specialdata = NULL;
			P_RemoveThinker (&activeceilings[i]->thinker);
			activeceilings[i] = NULL;
			return;
		}
	}

    // [d64] added error message
    I_Error("P_RemoveActiveCeiling: ceiling not found");
}



//
// P_ActivateInStasisCeiling
// Restart a ceiling that's in-stasis
//

void P_ActivateInStasisCeiling(line_t* line)
{
	int	i;
    for(i = 0; i < MAXCEILINGS; i++)
    {
		if(activeceilings[i] && (activeceilings[i]->tag == line->tag)
			&& (activeceilings[i]->direction == 0))
		{
			activeceilings[i]->direction = activeceilings[i]->olddirection;
			activeceilings[i]->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
		}
	}
}



//
// EV_CeilingCrushStop
// Stop a ceiling from crushing!
//

int	EV_CeilingCrushStop(line_t* line)
{
	int		i;
	int		rtn;

	rtn = 0;
	for (i = 0; i < MAXCEILINGS; i++)
    {
		if(activeceilings[i] && (activeceilings[i]->tag == line->tag)
			&& (activeceilings[i]->direction != 0))
		{
			activeceilings[i]->olddirection = activeceilings[i]->direction;
			activeceilings[i]->thinker.function.acv = (actionf_v)NULL;
			activeceilings[i]->direction = 0;		// in-stasis
			rtn = 1;
		}
	}
	
	return rtn;
}