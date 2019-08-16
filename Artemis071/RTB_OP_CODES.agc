### FILE="Main.annotation"
## Copyright:   Public domain.
## Filename:    RTB_OP_CODES.agc
## Purpose:     A section of Artemis revision 071.
##              It is part of the reconstructed source code for the first
##              release of the flight software for the Command Module's
##              (CM) Apollo Guidance Computer (AGC) for Apollo 15 through
##              17. The code has been recreated from a copy of Artemis 072.
##              It has been adapted such that the resulting bugger words
##              exactly match those specified for Artemis 071 in NASA
##              drawing 2021154-, which gives relatively high confidence
##              that the reconstruction is correct.
## Reference:   1501
## Assembler:   yaYUL
## Contact:     Ron Burkey <info@sandroid.org>.
## Website:     www.ibiblio.org/apollo/index.html
## Mod history: 2019-08-14 MAS  Created from Artemis 072.

## Page 1501

		SETLOC	RTBCODE1
		BANK

		EBANK=	XNB
		COUNT*	$$/RTB

# LOAD TIME2, TIME1 INTO MPAC:

LOADTIME	EXTEND
		DCA	TIME2
		TCF	SLOAD2

# CONVERT THE SINGLE PRECISION 2'S COMPLEMENT NUMBER ARRIVING IN MPAC (SCALED IN HALF-REVOLUTIONS) TO A
# DP 1'S COMPLEMENT NUMBER SCALED IN REVOLUTIONS.

CDULOGIC	CCS	MPAC
		CAF	ZERO
		TCF	+3
		NOOP
		CS	HALF

		TS	MPAC +1
		CAF	ZERO
		XCH	MPAC
		EXTEND
		MP	HALF
		DAS	MPAC
		TCF	DANZIG		# MODE IS ALREADY AT DOUBLE-PRECISION

		SETLOC	RTBCODES
		BANK

		COUNT*	$$/RTB

# READ THE PIPS INTO MPAC WITHOUT CHANGING THEM:

READPIPS	INHINT
		CA	PIPAX
		TS	MPAC
		CA	PIPAY
		TS	MPAC +3
		CA	PIPAZ
		RELINT
		TS	MPAC +5
		
		CAF	ZERO
		TS	MPAC +1
		TS	MPAC +4
		TS	MPAC +6
VECMODE		TCF	VMODE

## Page 1502

# FORCE TP SIGN AGREEMENT IN MPAC:

SGNAGREE	TC	TPAGREE
		TCF	DANZIG

# CONVERT THE DP 1'S COMPLEMENT ANGLE SCALED IN REVOLUTIONS TO A SINGLE PRECISION 2'S COMPLEMENT ANGLE
# SCALED IN HALF-REVOLUTIONS.

1STO2S		TC	1TO2SUB
		CAF	ZERO
		TS	MPAC +1
		TCF	NEWMODE

# DO 1STO2S ON A VECTOR OF ANGLES:

V1STO2S		TC	1TO2SUB		# ANSWER ARRIVES IN A AND MPAC.

		DXCH	MPAC +5
		DXCH	MPAC
		TC	1TO2SUB
		TS	MPAC +2

		DXCH	MPAC +3
		DXCH	MPAC
		TC	1TO2SUB
		TS	MPAC +1

		CA	MPAC +5
		TS	MPAC

TPMODE		CAF	ONE		# MODE IS TP.
		TCF	NEWMODE

# SUBROUTINE TO DO DOUBLING AND 1'S TO 2'S CONVERSION:

1TO2SUB		DXCH	MPAC		# FINAL MPAC +1 UNSPECIFIED.
		DDOUBL
		CCS	A
		AD	ONE
		TCF	+2
		COM			# THIS WAS REVERSE OF MSU.

		TS	MPAC		# AND SKIP ON OVERFLOW.
		TC	Q

		INDEX	A		# OVERFLOW UNCORRECT AND IN MSU.
		CAF	LIMITS
		ADS	MPAC
		TC	Q

## Page 1503

# THE FOLLOWING ROUTINE INCREMENTS IN 2S COMPLEMENT THE REGISTER WHOSE ADDRESS IS IN BUF BY THE 1S COMPL.
# QUANTITY FOUND IN TEM2. THIS MAY BE USED TO INCREMENT DESIRED IMU AND OPTICS CDU ANGLES OR ANY OTHER 2S COMPL.
# (+0 UNEQUAL TO -0) QUANTITY. MAY BE CALLED BY BANKCALL/SWCALL.

CDUINC		TS	TEM2		# 1S COMPL.QUANT. ARRIVES IN ACC. STORE IT
		INDEX	BUF
		CCS	0		# CHANGE 2S COMPL. ANGLE(IN BUF)INTO 1S
		AD	ONE
		TCF	+4
		AD	ONE
		AD	ONE		# OVERFLOW HERE IF 2S COMPL. IS 180 DEG.
		COM

		AD	TEM2		# SULT MOVES FROM 2ND TO 3D QUAD. (OR BACK)
		CCS	A		# BACK TO 2S COMPL.
		AD	ONE
		TCF	+2
		COM
		TS	TEM2		# STORE 14BIT QUANTITY WITH PRESENT SIGN
		TCF	+4
		INDEX	A		# SIGN.
		CAF	LIMITS		# FIX IT, BY ADDING IN 37777 OR 40000
		AD	TEM2

		INDEX	BUF
		TS	0		# STORE NEW ANGLE IN 2S COMPLEMENT.
		TC	Q

## Page 1504

# RTB TO TORQUE GYROS, EXCEPT FOR THE CALL TO IMUSTALL. ECADR OF COMMANDS ARRIVES IN X1.

PULSEIMU	INDEX	FIXLOC		# ADDRESS OF GYRO COMMANDS SHOULD BE IN X1
		CA	X1
		TC	BANKCALL
		CADR	IMUPULSE
		TCF	DANZIG

## Page 1505

# EACH ROUTINE TAKES A 3X3 MATRIX STORED IN DOUBLE PRECISION IN A FIXED AREA OF ERASABLE MEMORY AND REPLACES IT
# WITH THE TRANSPOSE MATRIX. TRANSP1 USES LOCATIONS XNB+0,+1 THROUGH XNB+16D, 17D AND TRANSP2 USES LOCATIONS
# XNB1+0,+1 THROUGH XNB1+16D, 17D. EACH MATRIX IS STORED BY ROWS.

XNBEB		ECADR	XNB
XNB1EB		ECADR	XNB1

		EBANK=	XNB
		
TRANSP1		CAF	XNBEB
		TS	EBANK
		DXCH	XNB +2
		DXCH	XNB +6
		DXCH	XNB +2
		
		DXCH	XNB +4
		DXCH	XNB +12D
		DXCH	XNB +4
		
		DXCH	XNB +10D
		DXCH	XNB +14D
		DXCH	XNB +10D
		TCF	DANZIG

		EBANK=	XNB1
		
TRANSP2		CAF	XNB1EB
		TS	EBANK
		DXCH	XNB1 +2
		DXCH	XNB1 +6
		DXCH	XNB1 +2
		
		DXCH	XNB1 +4
		DXCH	XNB1 +12D
		DXCH	XNB1 +4
		
		DXCH	XNB1 +10D
		DXCH	XNB1 +14D
		DXCH	XNB1 +10D
		TCF	DANZIG

## Page 1506

# THE SUBROUTINE  SIGNMPAC  SETS C(MPAC, MPAC +1) TO SIGN(MPAC).
# FOR THIS, ONLY THE CONTENTS OF MPAC ARE EXAMINED. ALSO +0 YIELDS POSMAX AND -0 YIELDS NEGMAX.
#
# ENTRY MAY BE BY EITHER OF THE FOLLOWING:
#
#	1.	LIMIT THE SIZE OF MPAC ON INTERPRETIVE OVERFLOW:
#	ENTRY:		BOVB
#				SIGNMPAC
#	2.	GENERATE IN MPAC THE SIGNUM FUNCTION OF MPAC:
#	ENTRY:		RTB
#				SIGNMPAC
#
# IN EITHER CASE, RETURN IS TO THE NEXT INTERPRETIVE INSTRUCTION IN THE CALLING SEQUENCE.

SIGNMPAC	EXTEND
		DCA	DPOSMAX
		DXCH	MPAC
		CCS	A
DPMODE		CAF	ZERO		# SETS MPAC +2 TO ZERO IN THE PROCESS
		TCF	SLOAD2 +2
		TCF	+1
		EXTEND
		DCS	DPOSMAX
		TCF	SLOAD2


