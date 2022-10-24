/*  This file is part of UKNCBTL.
UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// SoundAY.cpp
//
// Emulation of the AY-3-8910 / YM2149 sound chip.
// Based on various code snippets by Ville Hallik, Michael Cuddy,
// Tatsuyuki Satoh, Fabrice Frances, Nicola Salmoria.

#include "stdafx.h"
#include "Emubase.h"

#define MAX_OUTPUT 0x0ffff

#define STEP3 1
#define STEP2 length
#define STEP  2

/* register id's */
#define AY_AFINE    (0)
#define AY_ACOARSE  (1)
#define AY_BFINE    (2)
#define AY_BCOARSE  (3)
#define AY_CFINE    (4)
#define AY_CCOARSE  (5)
#define AY_NOISEPER (6)
#define AY_ENABLE   (7)
#define AY_AVOL     (8)
#define AY_BVOL     (9)
#define AY_CVOL     (10)
#define AY_EFINE    (11)
#define AY_ECOARSE  (12)
#define AY_ESHAPE   (13)

#define AY_PORTA    (14)
#define AY_PORTB    (15)


unsigned int CSoundAY::VolTable[32];


CSoundAY::CSoundAY()
{
    Reset();
}

void CSoundAY::Reset()
{
    index = 0;
    memset(Regs, 0, sizeof(Regs));
    lastEnable = 0;
    PeriodA = PeriodB = PeriodC = PeriodN = PeriodE = STEP3;
    CountA = CountB = CountC = CountN = CountE = 0;
    VolA = VolB = VolC = VolE = 0;
    EnvelopeA = EnvelopeB = EnvelopeC = 0;
    OutputA = OutputB = OutputC = 0;
    OutputN = 0xff;
    CountEnv = 0;
    Hold = Alternate = Attack = Holding = 0;
    RNG = 1;

    BuildMixerTable();

    ready = 1;
}

void CSoundAY::SetReg(int r, int v)
{
    int old;

    this->Regs[r] = v;

    /* A note about the period of tones, noise and envelope: for speed reasons,*/
    /* we count down from the period to 0, but careful studies of the chip     */
    /* output prove that it instead counts up from 0 until the counter becomes */
    /* greater or equal to the period. This is an important difference when the*/
    /* program is rapidly changing the period to modulate the sound.           */
    /* To compensate for the difference, when the period is changed we adjust  */
    /* our internal counter.                                                   */
    /* Also, note that period = 0 is the same as period = 1. This is mentioned */
    /* in the YM2203 data sheets. However, this does NOT apply to the Envelope */
    /* period. In that case, period = 0 is half as period = 1. */
    switch ( r )
    {
    case AY_AFINE:
    case AY_ACOARSE:
        this->Regs[AY_ACOARSE] &= 0x0f;
        old = this->PeriodA;
        this->PeriodA = (this->Regs[AY_AFINE] + 256 * this->Regs[AY_ACOARSE]) * STEP3;
        if (this->PeriodA == 0) this->PeriodA = STEP3;
        this->CountA += this->PeriodA - old;
        if (this->CountA <= 0) this->CountA = 1;
        break;
    case AY_BFINE:
    case AY_BCOARSE:
        this->Regs[AY_BCOARSE] &= 0x0f;
        old = this->PeriodB;
        this->PeriodB = (this->Regs[AY_BFINE] + 256 * this->Regs[AY_BCOARSE]) * STEP3;
        if (this->PeriodB == 0) this->PeriodB = STEP3;
        this->CountB += this->PeriodB - old;
        if (this->CountB <= 0) this->CountB = 1;
        break;
    case AY_CFINE:
    case AY_CCOARSE:
        this->Regs[AY_CCOARSE] &= 0x0f;
        old = this->PeriodC;
        this->PeriodC = (this->Regs[AY_CFINE] + 256 * this->Regs[AY_CCOARSE]) * STEP3;
        if (this->PeriodC == 0) this->PeriodC = STEP3;
        this->CountC += this->PeriodC - old;
        if (this->CountC <= 0) this->CountC = 1;
        break;
    case AY_NOISEPER:
        this->Regs[AY_NOISEPER] &= 0x1f;
        old = this->PeriodN;
        this->PeriodN = this->Regs[AY_NOISEPER] * STEP3;
        if (this->PeriodN == 0) this->PeriodN = STEP3;
        this->CountN += this->PeriodN - old;
        if (this->CountN <= 0) this->CountN = 1;
        break;
    case AY_ENABLE:
        this->lastEnable = this->Regs[AY_ENABLE];
        break;
    case AY_AVOL:
        this->Regs[AY_AVOL] &= 0x1f;
        this->EnvelopeA = this->Regs[AY_AVOL] & 0x10;
        this->VolA = this->EnvelopeA ? this->VolE : this->VolTable[this->Regs[AY_AVOL] ? this->Regs[AY_AVOL] * 2 + 1 : 0];
        break;
    case AY_BVOL:
        this->Regs[AY_BVOL] &= 0x1f;
        this->EnvelopeB = this->Regs[AY_BVOL] & 0x10;
        this->VolB = this->EnvelopeB ? this->VolE : this->VolTable[this->Regs[AY_BVOL] ? this->Regs[AY_BVOL] * 2 + 1 : 0];
        break;
    case AY_CVOL:
        this->Regs[AY_CVOL] &= 0x1f;
        this->EnvelopeC = this->Regs[AY_CVOL] & 0x10;
        this->VolC = this->EnvelopeC ? this->VolE : this->VolTable[this->Regs[AY_CVOL] ? this->Regs[AY_CVOL] * 2 + 1 : 0];
        break;
    case AY_EFINE:
    case AY_ECOARSE:
        old = this->PeriodE;
        this->PeriodE = ((this->Regs[AY_EFINE] + 256 * this->Regs[AY_ECOARSE])) * STEP3;
        //if (this->PeriodE == 0) this->PeriodE = STEP3 / 2;
        if (this->PeriodE == 0) this->PeriodE = STEP3;
        this->CountE += this->PeriodE - old;
        if (this->CountE <= 0) this->CountE = 1;
        break;
    case AY_ESHAPE:
        /* envelope shapes:
        C AtAlH
        0 0 x x  \___

        0 1 x x  /___

        1 0 0 0  \\\\

        1 0 0 1  \___

        1 0 1 0  \/\/
                  ___
        1 0 1 1  \

        1 1 0 0  ////
                  ___
        1 1 0 1  /

        1 1 1 0  /\/\

        1 1 1 1  /___

        The envelope counter on the AY-3-8910 has 16 steps. On the YM2149 it
        has twice the steps, happening twice as fast. Since the end result is
        just a smoother curve, we always use the YM2149 behaviour.
        */
        this->Regs[AY_ESHAPE] &= 0x0f;
        this->Attack = (this->Regs[AY_ESHAPE] & 0x04) ? 0x1f : 0x00;
        if ((this->Regs[AY_ESHAPE] & 0x08) == 0)
        {
            /* if Continue = 0, map the shape to the equivalent one which has Continue = 1 */
            this->Hold = 1;
            this->Alternate = this->Attack;
        }
        else
        {
            this->Hold = this->Regs[AY_ESHAPE] & 0x01;
            this->Alternate = this->Regs[AY_ESHAPE] & 0x02;
        }
        this->CountE = this->PeriodE;
        this->CountEnv = 0x1f;
        this->Holding = 0;
        this->VolE = this->VolTable[this->CountEnv ^ this->Attack];
        if (this->EnvelopeA) this->VolA = this->VolE;
        if (this->EnvelopeB) this->VolB = this->VolE;
        if (this->EnvelopeC) this->VolC = this->VolE;
        break;
    case AY_PORTA:
        break;
    case AY_PORTB:
        break;
    }
}

void CSoundAY::Callback(uint8_t* stream, int length)
{
    int outn;
    uint8_t* buf1 = stream;

    /* hack to prevent us from hanging when starting filtered outputs */
    if (!this->ready)
    {
        memset(stream, 0, length * sizeof(*stream));
        return;
    }

    length = length * 2;

    /* The 8910 has three outputs, each output is the mix of one of the three */
    /* tone generators and of the (single) noise generator. The two are mixed */
    /* BEFORE going into the DAC. The formula to mix each channel is: */
    /* (ToneOn | ToneDisable) & (NoiseOn | NoiseDisable). */
    /* Note that this means that if both tone and noise are disabled, the output */
    /* is 1, not 0, and can be modulated changing the volume. */


    /* If the channels are disabled, set their output to 1, and increase the */
    /* counter, if necessary, so they will not be inverted during this update. */
    /* Setting the output to 1 is necessary because a disabled channel is locked */
    /* into the ON state (see above); and it has no effect if the volume is 0. */
    /* If the volume is 0, increase the counter, but don't touch the output. */
    if (this->Regs[AY_ENABLE] & 0x01)
    {
        if (this->CountA <= STEP2) this->CountA += STEP2;
        this->OutputA = 1;
    }
    else if (this->Regs[AY_AVOL] == 0)
    {
        /* note that I do count += length, NOT count = length + 1. You might think */
        /* it's the same since the volume is 0, but doing the latter could cause */
        /* interferencies when the program is rapidly modulating the volume. */
        if (this->CountA <= STEP2) this->CountA += STEP2;
    }
    if (this->Regs[AY_ENABLE] & 0x02)
    {
        if (this->CountB <= STEP2) this->CountB += STEP2;
        this->OutputB = 1;
    }
    else if (this->Regs[AY_BVOL] == 0)
    {
        if (this->CountB <= STEP2) this->CountB += STEP2;
    }
    if (this->Regs[AY_ENABLE] & 0x04)
    {
        if (this->CountC <= STEP2) this->CountC += STEP2;
        this->OutputC = 1;
    }
    else if (this->Regs[AY_CVOL] == 0)
    {
        if (this->CountC <= STEP2) this->CountC += STEP2;
    }

    /* for the noise channel we must not touch OutputN - it's also not necessary */
    /* since we use outn. */
    if ((this->Regs[AY_ENABLE] & 0x38) == 0x38) /* all off */
        if (this->CountN <= STEP2) this->CountN += STEP2;

    outn = (this->OutputN | this->Regs[AY_ENABLE]);

    /* buffering loop */
    while (length > 0)
    {
        unsigned vol;
        int left  = 2;
        /* vola, volb and volc keep track of how long each square wave stays */
        /* in the 1 position during the sample period. */

        int vola, volb, volc;
        vola = volb = volc = 0;

        do
        {
            int nextevent;

            if (this->CountN < left) nextevent = this->CountN;
            else nextevent = left;

            if (outn & 0x08)
            {
                if (this->OutputA) vola += this->CountA;
                this->CountA -= nextevent;
                /* PeriodA is the half period of the square wave. Here, in each */
                /* loop I add PeriodA twice, so that at the end of the loop the */
                /* square wave is in the same status (0 or 1) it was at the start. */
                /* vola is also incremented by PeriodA, since the wave has been 1 */
                /* exactly half of the time, regardless of the initial position. */
                /* If we exit the loop in the middle, OutputA has to be inverted */
                /* and vola incremented only if the exit status of the square */
                /* wave is 1. */
                while (this->CountA <= 0)
                {
                    this->CountA += this->PeriodA;
                    if (this->CountA > 0)
                    {
                        this->OutputA ^= 1;
                        if (this->OutputA) vola += this->PeriodA;
                        break;
                    }
                    this->CountA += this->PeriodA;
                    vola += this->PeriodA;
                }
                if (this->OutputA) vola -= this->CountA;
            }
            else
            {
                this->CountA -= nextevent;
                while (this->CountA <= 0)
                {
                    this->CountA += this->PeriodA;
                    if (this->CountA > 0)
                    {
                        this->OutputA ^= 1;
                        break;
                    }
                    this->CountA += this->PeriodA;
                }
            }

            if (outn & 0x10)
            {
                if (this->OutputB) volb += this->CountB;
                this->CountB -= nextevent;
                while (this->CountB <= 0)
                {
                    this->CountB += this->PeriodB;
                    if (this->CountB > 0)
                    {
                        this->OutputB ^= 1;
                        if (this->OutputB) volb += this->PeriodB;
                        break;
                    }
                    this->CountB += this->PeriodB;
                    volb += this->PeriodB;
                }
                if (this->OutputB) volb -= this->CountB;
            }
            else
            {
                this->CountB -= nextevent;
                while (this->CountB <= 0)
                {
                    this->CountB += this->PeriodB;
                    if (this->CountB > 0)
                    {
                        this->OutputB ^= 1;
                        break;
                    }
                    this->CountB += this->PeriodB;
                }
            }

            if (outn & 0x20)
            {
                if (this->OutputC) volc += this->CountC;
                this->CountC -= nextevent;
                while (this->CountC <= 0)
                {
                    this->CountC += this->PeriodC;
                    if (this->CountC > 0)
                    {
                        this->OutputC ^= 1;
                        if (this->OutputC) volc += this->PeriodC;
                        break;
                    }
                    this->CountC += this->PeriodC;
                    volc += this->PeriodC;
                }
                if (this->OutputC) volc -= this->CountC;
            }
            else
            {
                this->CountC -= nextevent;
                while (this->CountC <= 0)
                {
                    this->CountC += this->PeriodC;
                    if (this->CountC > 0)
                    {
                        this->OutputC ^= 1;
                        break;
                    }
                    this->CountC += this->PeriodC;
                }
            }

            this->CountN -= nextevent;
            if (this->CountN <= 0)
            {
                /* Is noise output going to change? */
                if ((this->RNG + 1) & 2)    /* (bit0^bit1)? */
                {
                    this->OutputN = ~this->OutputN;
                    outn = (this->OutputN | this->Regs[AY_ENABLE]);
                }

                /* The Random Number Generator of the 8910 is a 17-bit shift */
                /* register. The input to the shift register is bit0 XOR bit3 */
                /* (bit0 is the output). This was verified on AY-3-8910 and YM2149 chips. */

                /* The following is a fast way to compute bit17 = bit0^bit3. */
                /* Instead of doing all the logic operations, we only check */
                /* bit0, relying on the fact that after three shifts of the */
                /* register, what now is bit3 will become bit0, and will */
                /* invert, if necessary, bit14, which previously was bit17. */
                if (this->RNG & 1) this->RNG ^= 0x24000; /* This version is called the "Galois configuration". */
                this->RNG >>= 1;
                this->CountN += this->PeriodN;
            }

            left -= nextevent;
        }
        while (left > 0);

        /* update envelope */
        if (this->Holding == 0)
        {
            this->CountE -= STEP;
            if (this->CountE <= 0)
            {
                do
                {
                    this->CountEnv--;
                    this->CountE += this->PeriodE;
                }
                while (this->CountE <= 0);

                /* check envelope current position */
                if (this->CountEnv < 0)
                {
                    if (this->Hold)
                    {
                        if (this->Alternate)
                            this->Attack ^= 0x1f;
                        this->Holding = 1;
                        this->CountEnv = 0;
                    }
                    else
                    {
                        /* if CountEnv has looped an odd number of times (usually 1), */
                        /* invert the output. */
                        if (this->Alternate && (this->CountEnv & 0x20))
                            this->Attack ^= 0x1f;

                        this->CountEnv &= 0x1f;
                    }
                }

                this->VolE = this->VolTable[this->CountEnv ^ this->Attack];
                /* reload volume */
                if (this->EnvelopeA) this->VolA = this->VolE;
                if (this->EnvelopeB) this->VolB = this->VolE;
                if (this->EnvelopeC) this->VolC = this->VolE;
            }
        }

        vol = (vola * this->VolA + volb * this->VolB + volc * this->VolC) / (3 * STEP);
        if (--length & 1) *(buf1++) = (uint8_t)(vol >> 8);
    }
}

void CSoundAY::BuildMixerTable()
{
    /* calculate the volume->voltage conversion table */
    /* The AY-3-8910 has 16 levels, in a logarithmic scale (3dB per STEP) */
    /* The YM2149 still has 16 levels for the tone generators, but 32 for */
    /* the envelope generator (1.5dB per STEP). */
    double out = MAX_OUTPUT;
    for (int i = 31; i > 0; i--)
    {
        VolTable[i] = (unsigned)(out + 0.5);    /* round to nearest */
        out /= 1.188502227; /* = 10 ^ (1.5/20) = 1.5dB */
    }
    VolTable[0] = 0;
}
