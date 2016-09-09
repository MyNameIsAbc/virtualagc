/*
 * Copyright 2016 Ronald S. Burkey <info@sandroid.org>
 *
 * This file is part of yaAGC.
 *
 * yaAGC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * yaAGC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with yaAGC; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * In addition, as a special exception, Ronald S. Burkey gives permission to
 * link the code of this program with the Orbiter SDK library (or with
 * modified versions of the Orbiter SDK library that use the same license as
 * the Orbiter SDK library), and distribute linked combinations including
 * the two. You must obey the GNU General Public License in all respects for
 * all of the code used other than the Orbiter SDK library. If you modify
 * this file, you may extend this exception to your version of the file,
 * but you are not obligated to do so. If you do not wish to do so, delete
 * this exception statement from your version.
 *
 * Filename:    executeOneInstruction.c
 * Purpose:     Executes a single instruction on the virtual AGC.
 * Compiler:    GNU gcc.
 * Contact:     Ron Burkey <info@sandroid.org>
 * Reference:   http://www.ibiblio.org/apollo/index.html
 * Mods:        2016-09-03 RSB  Wrote.
 *              2016-09-04 RSB  Fixed a bug in CCS decrementing.
 *              2016-09-09 RSB  Lots of fixes, particularly backing out
 *                              stuff that I was trying (pointlessly) to
 *                              fix discrepancies with yaAGC-Block1 that
 *                              were actually due to yaAGC-Block1 sticking
 *                              parity bits on everything. All the
 *                              instructions implemented now, though I've
 *                              had no way to test DV so far.
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "yaAGCb1.h"

// Write a value to an erasable address.
void
writeToErasableOrRegister(uint16_t flatAddress, uint16_t value)
{
  if (flatAddress >= 04 && !(flatAddress >= 035 && flatAddress <= 040))
    value &= 077777;
  if (&regBank== &agc.memory[flatAddress]) agc.memory[flatAddress] = value & 076000;
  else if (flatAddress < 02000) agc.memory[flatAddress] = value;
}

static int numMCT;
static uint16_t lastINDEX = 0;
static uint16_t lastZ = 0;
void
implementationError(char *message)
{
  agc.instructionCountDown = 1;
  printf("*** Implementation error: %s ***\n", message);
  numMCT = 0;
  regZ= lastZ;
  agc.INDEX = lastINDEX;
}

void
incrementZ(uint16_t increment)
{
  regZ= (regZ & 06000) + ((regZ + increment) & 01777);
}

uint16_t
fixUcForWriting(uint16_t valueFromAthroughLP)
{
  return (valueFromAthroughLP & 037777) | ((valueFromAthroughLP & 0100000) >> 1);
}

int
incTimerCheckOverflow(uint16_t *timer)
{
  agc.countMCT += 1;
  (*timer)++;
  if (*timer == 040000)
    {
      *timer = 0;
      return (1);
    }
  return (0);
}

void
edit(uint16_t flatAddress)
{
  if (flatAddress == 020)
    {
      regCYR= ((regCYR & 1) << 14) | ((regCYR & 077777) >> 1);
    }
  else if (flatAddress == 021)
    {
      regSR = (regSR & 040000) | (regSR >> 1);
    }
  else if (flatAddress == 022)
    {
      regCYL= ((regCYL & 040000) >> 14) | ((regCYL << 1) & 077777);
    }
  else if (flatAddress == 023)
    {
      regSL = ((regSL << 1) & 0077776) | ((regSL & 0100000) >> 15);
    }
  return;
}

void
executeOneInstruction(FILE *logFile)
{
  int instruction;
  int16_t term1, term2, sum;
  uint16_t flatAddress, opcode, operand, extracode, dummy, fetchedFromOperand,
      fetchedFromOperandSignExtended, fetchedOperandOverflow;
  static uint16_t lastInstruction = 0;

  numMCT = 2;

  retry: ;
  // Various preliminary stuff that it's just useful to do for any
  // instruction.
  flatAddress = (regZ< 06000) ? regZ : flatten(regBank, regZ);
  // Increment Z to next location.
  lastZ = regZ;
  incrementZ(1);
  lastINDEX = agc.INDEX;
  term1 = agc.memory[flatAddress];
  if (flatAddress < 4)
    term1 = fixUcForWriting(term1);
  term2 = SignExtend(agc.INDEX);
  // Special case:  note that x + -x = -0.
  if (077777 & term1 == 077777 & ~term2)
    sum = 0177777;
  else
    {
      if ((term1 & 040000) != 0)
        term1 = -(~(term1 | ~077777));
      if ((term2 & 040000) != 0)
        term2 = -(~term2);
      sum = term1 + term2;
      if (sum < 0)
        sum = ~(-sum);
    }
  instruction = sum;
  instruction &= 0177777;

  resumeFromInterrupt: ;
  agc.B = instruction;
  if (0 == (agc.B & 0100000))
    agc.B |= (agc.B & 040000) << 1;
  if (logFile != NULL)
    logAGC(logFile, lastZ);
  lastInstruction = instruction;
  agc.INDEX = 0;
  extracode = instruction & 0100000;
  opcode = instruction & 0070000;
  operand = instruction & 007777;

  // Prioritized interrupt vectors.
  if (!regInhint&& !agc.INTERRUPTED && 0100000 != (0140000 & regA) && 0040000 != (0140000 & regA)
  && !extracode)
    {
      uint16_t interruptVector = 0;

      // Test for interrupt triggers.
      if (agc.overflowedTIME3)// T3RUPT
        {
          interruptVector = 02000;
          agc.overflowedTIME3 = 0;
        }

      if (!interruptVector && 0 != (regIN2 & 077600))  // ERRUPT -- 8 fail bits in IN2.
        {
          interruptVector = 02004;
        }

      if (!interruptVector && agc.overflowedTIME4)  // DSRUPT
        {
          interruptVector = 02010;
          agc.overflowedTIME4 = 0;
        }

      if (!interruptVector && 0)  // KEYRUPT
        {
          interruptVector = 02014;
        }

      if (!interruptVector && 0)  // UPRUPT
        {
          interruptVector = 02020;
        }

      if (!interruptVector && 0)  // DOWNRUPT
        {
          interruptVector = 02024;
        }

      // Vector to the interrupt if necessary.
      if (interruptVector != 0)
        {
          agc.ruptFlatAddress = flatAddress;
          agc.ruptLastINDEX = lastINDEX;
          agc.ruptLastZ = lastZ;
          agc.countMCT += 3;
          agc.INTERRUPTED = 1;
          regZRUPT = regZ;
          regBRUPT = instruction;
          regZ = interruptVector;
          goto retry;
        }
    }

  if (operand < 06000)
    fetchedFromOperand = agc.memory[operand];
  else
    fetchedFromOperand = agc.memory[flatten(regBank, operand)];
  if (operand < 04) // Already has a UC bit.
    fetchedFromOperandSignExtended = fetchedFromOperand;
  else
    fetchedFromOperandSignExtended = ((fetchedFromOperand & 040000) << 1)
        | (fetchedFromOperand & 077777);
  fetchedOperandOverflow = fetchedFromOperandSignExtended & 0140000;

  // Now execute the stuff specific to the opcode.
  if (opcode == 0000000) /* TC */
    {
      uint16_t oldZ;
      // Recall that Q, Z, and the instruction we generated above are
      // correctly set up vis-a-vis the 16th bit.  The conditional is to
      // prevent Q from being overwritten in case the instruction is "TC Q"
      // ("RETURN"). This is a special case described on p. 3-9 of R-393.
      // R-393 says that "TC Q" takes 2 MCT; Pultorak says 1 MCT; I believe
      // Pultorak.
      numMCT = 1;
      if (instruction != 1)
        {
          regQ= regZ;
          numMCT = 1;
        }
      regZ= operand;
    }
  else if (opcode == 010000 && !extracode) /* CCS */
    {
      if (operand >= 02000) implementationError("CCS accessing fixed memory.");
      else
        {
          uint16_t K;
          // Arrange to jump.  Recall that Z already points to the next
          // instruction.
          K = (operand >= 04) ? fetchedFromOperand : fixUcForWriting(fetchedFromOperand);
          if (K == 000000) incrementZ(1);// +0
          else if (0 == (K & 040000)) incrementZ(0);// >0
          else if (K == 077777) incrementZ(3);// -0
          else incrementZ(2);// < 0
          // Compute the "diminished absolute value" of c(K).
          if (0 != (K & 040000)) K = (~K) & 037777;// Absolute value.
          if (K >= 1)
          K--;
          regA = K;
          edit(operand);
        }
    }
  else if (opcode == 020000 && !extracode) /* INDEX */
    {
      if (operand == 016) regInhint = 0;
      else if (operand == 017) regInhint = 1;
      else if (operand == 025)
        {
          if (!agc.INTERRUPTED)
          implementationError("RESUME without RUPT.\n");
          flatAddress = agc.ruptFlatAddress;
          lastINDEX = agc.ruptLastINDEX;
          lastZ = agc.ruptLastZ;
          agc.INTERRUPTED = 0;
          regZ = regZRUPT;
          instruction = regBRUPT;
          agc.countMCT += 2;
          goto resumeFromInterrupt;
        }
      else agc.INDEX = fetchedFromOperand;
    }
  else if (opcode == 030000 && !extracode) /* XCH (erasable) or CAF (fixed). */
    {
      if (operand >= 020 && operand <= 023)
        {
          agc.memory[operand] = fixUcForWriting(regA);
          regA = fetchedFromOperandSignExtended;
          edit(operand);
        }
      else if (operand < 04)
        {
          // Full 16-bit.
          agc.memory[operand] = regA;
          regA = fetchedFromOperand;
        }
      else
        {
          // Cannot actually write to regIN
          if (operand < 04 || operand > 07) writeToErasableOrRegister(operand, fixUcForWriting(regA));
          regA = fetchedFromOperandSignExtended;
        }
    }
  else if (opcode == 0040000) /* CS. */
    {
      regA = (~fetchedFromOperandSignExtended) & 0177777;
      edit(operand);
    }
  else if (opcode == 0050000) /* TS. */
    {
      if (operand >= 02000) implementationError("TS accessing fixed memory.");
      else
        {
          uint16_t aOverflowBits;
          int value;
          aOverflowBits = regA & 0140000;
          if (operand >= 020 && operand <= 023)
            {
              agc.memory[operand] = regA;
              edit(operand);
            }
          else
            {
              value = fixUcForWriting(regA);
              writeToErasableOrRegister(operand, value);
            }
          if (aOverflowBits == 0100000 || aOverflowBits == 0040000)
            {
              // Overflow.
              incrementZ(1);
              if (0 & (fetchedOperandOverflow & 0100000)) regA = 0000001;// Positive overflow.
              else regA = 0177776;// Negative overflow;
            }
        }
    }
  else if (opcode == 060000) /* AD. */
    {
      // FIXME Not sure what's supposed to happen if A already has
      // overflowed.  For now, let's make sure it hasn't.
      int16_t term1, term2, sum;
      entrySubtraction:;
#if 1
      term1 = fixUcForWriting(regA);
      term2 = fetchedFromOperandSignExtended;
      // Special case:  note that x + -x = -0.
      if ((077777 & term1) == (077777 & ~term2)) sum = 0177777;
      // And -0 + -0 = -0 too, for some reason.
      else if ((077777 & term1) == 077777 && (077777 & term2) == 077777) sum = 0177777;
      else
        {
          if ((term1 & 040000) != 0) term1 = -(~(term1 | ~077777));
          if ((term2 & 040000) != 0) term2 = -(~term2);
          sum = term1 + term2;
          if (sum < 0 ) sum = ~(-sum);
        }
#else
      sum = AddSP16 (SignExtend(regA), SignExtend(fetchedFromOperand));
#endif
      regA = sum;
      if ((sum & 0140000) == 0040000) // Positive overflow
        {
          numMCT++;
          ctrOVCTR = (ctrOVCTR + 1) & 077777;
          if (ctrOVCTR == 077777) ctrOVCTR = 0; // Convert -0 to +0.
        }
      else if ((sum & 0140000) == 0100000) // Negative overflow
        {
          numMCT++;
          if (ctrOVCTR == 0) ctrOVCTR = 077777; // Convert +0 to -0.
          ctrOVCTR = (ctrOVCTR - 1) & 077777;
        }
    }
  else if (opcode == 070000) /* MASK. */
    {
      regA &= fetchedFromOperandSignExtended;
    }
  else if (opcode == 010000 && extracode) /* MP */
    {
      numMCT = 8;

      // Unlike almost everything else in this program, this is adapted
      // (copied) from the original Block 2 yaAGC.

      // For MP A (i.e., SQUARE) the accumulator is NOT supposed to
      // be overflow-corrected.  I do it anyway, since I don't know
      // what it would mean to carry out the operation otherwise.
      // Fix later if it causes a problem.
      // FIX ME: Accumulator is overflow-corrected before SQUARE.
      int16_t MsWord, LsWord, Operand16, OtherOperand16;
      int Product;
      Operand16 = fixUcForWriting(regA);
      OtherOperand16 = fetchedFromOperandSignExtended;
      if (OtherOperand16 == AGC_P0 || OtherOperand16 == AGC_M0)
      MsWord = LsWord = AGC_P0;
      else if (Operand16 == AGC_P0 || Operand16 == AGC_M0)
        {
          if ((Operand16 == AGC_P0 && 0 != (040000 & OtherOperand16)) ||
              (Operand16 == AGC_M0 && 0 == (040000 & OtherOperand16)))
          MsWord = LsWord = AGC_M0;
          else
          MsWord = LsWord = AGC_P0;
        }
      else
        {
          int16_t WordPair[2];
          Product =
          agc2cpu (SignExtend (Operand16)) *
          agc2cpu (SignExtend (OtherOperand16));
          Product = cpu2agc2 (Product);
          // Sign-extend, because it's needed for DecentToSp.
          if (02000000000 & Product)
          Product |= 004000000000;
          // Convert back to DP.
          DecentToSp (Product, &WordPair[1]);
          MsWord = WordPair[0];
          LsWord = WordPair[1];
        }
      regA = SignExtend (MsWord);
      regLP = SignExtend (LsWord);
    }
  else if (opcode == 020000 && extracode) /* DV */
    {
      numMCT = 18;

      {
        int16_t AccPair[2], AbsA, AbsL, AbsK, Div16, Operand16;
        int Dividend, Divisor, Quotient, Remainder;
        // Fetch the values;
        AccPair[0] = fixUcForWriting (regA);
        AccPair[1] = regLP;
        Dividend = SpToDecent (&AccPair[1]);
        DecentToSp (Dividend, &AccPair[1]);
        // Check boundary conditions.
        AbsA = AbsSP (AccPair[0]);
        AbsL = AbsSP (AccPair[1]);
        AbsK = AbsSP (fetchedFromOperandSignExtended);
        if (AbsA > AbsK || (AbsA == AbsK && AbsL != AGC_P0))
          {
            // The divisor is smaller than the dividend.  In this case,
            // we return "total nonsense".
            regLP = ~0;
            regA = 0;
          }
        else if (AbsA == AbsK && AbsL == AGC_P0)
          {
            // The divisor is equal to the dividend.
            if (AccPair[0] == fetchedFromOperandSignExtended)// Signs agree?
              {
                Operand16 = 037777;   // Max positive value.
                regLP = SignExtend (fetchedFromOperandSignExtended);
              }
            else
              {
                Operand16 = (077777 & ~037777);       // Max negative value.
                regLP = SignExtend (fetchedFromOperandSignExtended);
              }
            regA = SignExtend (Operand16);
          }
        else
          {
            // The divisor is larger than the dividend.  Okay to actually divide!
            // Fortunately, the sign conventions agree with those of the normal
            // C operators / and %, so all we need to do is to convert the
            // 1's-complement values to native CPU format to do the division,
            // and then convert back afterward.  Incidentally, we know we
            // aren't dividing by zero, since we know that the divisor is
            // greater (in magnitude) than the dividend.
            Dividend = agc2cpu2 (Dividend);
            Divisor = agc2cpu (fetchedFromOperandSignExtended);
            Quotient = Dividend / Divisor;
            Remainder = Dividend % Divisor;
            regA = SignExtend (cpu2agc (Quotient));
            if (Remainder == 0)
              {
                // In this case, we need to make an extra effort, because we
                // might need -0 rather than +0.
                if (Dividend >= 0) regLP = AGC_P0;
                else regLP = SignExtend (AGC_M0);
              }
            else
            regLP = SignExtend (cpu2agc (Remainder));
          }
      }
    }
  else if (opcode == 030000 && extracode) /* SU */
    {
      // R-393 says that SU takes 2 more MCT than AD, but the control-pulse
      // sequences it lists for SU don't support that notion.
      //numMCT += 2;
      fetchedFromOperandSignExtended = ~fetchedFromOperand;
      fetchedFromOperand = fetchedFromOperandSignExtended & 077777;
      goto entrySubtraction;
    }
  else
    {
      char message[64];
      sprintf (message, "Unimplemented opcode %05o (%06o)", opcode, extracode);
      implementationError(message);
    }

  agc.countMCT += numMCT;

  /*
   * Update regTIME1-4.  When TIME1 overflows it increments
   * TIME2.  TIME1,3,4 counts up every 10 ms.,
   * i.e., every 1024000/12/100 MCT = 2560/3 MCT.  The starting count is
   * set to half of this, for no particular reason.  John's original code
   * counted 10X too fast for some reason.
   */
    {
      static int nextTimerIncrement = 1280;
      int overflow = 0;
      if (agc.countMCT * 3 > nextTimerIncrement)
        {
          nextTimerIncrement += 2560;
          if (incTimerCheckOverflow(&ctrTIME1))
            {
              incTimerCheckOverflow (&ctrTIME2);
            }
          agc.overflowedTIME3 |= incTimerCheckOverflow(&ctrTIME3);
          agc.overflowedTIME4 |= incTimerCheckOverflow(&ctrTIME4);
        }

    }
}

