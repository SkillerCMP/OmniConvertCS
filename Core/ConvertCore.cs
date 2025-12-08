using System;

namespace OmniconvertCS
{
    /// <summary>
    /// Core Omniconvert pipeline (Decrypt -> Translate -> Encrypt).
    /// This is a C# port of the high-level logic from omniconvert.c, with
    /// crypto dispatch for AR1/AR2/ARMAX/CB/GS3 wired in.
    ///
    /// NOTE: The actual device-to-device opcode translation (SCF/translate.c)
    /// is still TODO; TranslateCode() currently only handles ARMAX's verifier
    /// add/remove logic.
    /// </summary>
    public static class ConvertCore
    {
        // === Enumerations mirroring the C-side concepts ===

        public enum Device
        {
            DEV_AR1,
            DEV_AR2,
            DEV_ARMAX,
            DEV_CB,
            DEV_GS3,
            DEV_STD
        }

        public enum Crypt
        {
            CRYPT_AR1,
            CRYPT_AR2,
            CRYPT_ARMAX,
            CRYPT_CB,
            CRYPT_CB7_COMMON,
            CRYPT_GS3,
            CRYPT_GS5,
            CRYPT_MAXRAW,
            CRYPT_RAW,
			// New: device-specific RAW aliases
			CRYPT_ARAW,  // AR2 RAW
			CRYPT_CRAW,  // CodeBreaker RAW
			CRYPT_GRAW   // GS3 RAW
        }

        public enum VerifierMode
        {
            AUTO,
            MANUAL
        }

        // === Global-ish configuration (mirrors g_* from C) ===

        public static Crypt g_incrypt;
        public static Crypt g_outcrypt;
        public static Device g_indevice;
        public static Device g_outdevice;
		public static int g_cbcSaveVersion = 7; // 7 = v7 (Day1), 8 = v8 (CFU)

        public static VerifierMode g_verifiermode = VerifierMode.AUTO;

        // Game metadata used by ARMAX verifier lines.
        public static uint g_gameid;
        public static uint g_region;
		
		// ARMAX options
		// Make Organizers / folder linking (mirrors g_makefolders in C).
		public static bool g_makefolders;

		// ARMAX disc hash drive letter (0 = none; otherwise 'A'..'Z').
		public static char g_hashdrive = '\0';

        // Seeds/keys.  In the original C, armseeds is 0x04030209 by default.
        public static uint ar2seeds = Ar2.AR1_SEED;   // default = AR1_SEED, like the C code
		public static uint armseeds = 0x04030209u;


        // GS3/GS5 key index (1–7); passed through to Gs3.BatchEncrypt().
        public static uint g_gs3key;

        /// <summary>
        /// Port of int DecryptCode(cheat_t *cheat).
        /// Dispatches based on g_incrypt.
        /// </summary>
        public static int DecryptCode(cheat_t cheat)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));
            if (cheat.codecnt == 0)
                return 0;
			// Mirror C's ProcessText(): reset AR2 seed before each conversion run.
			// (In C: ar2SetSeed(ar2seeds);)
			Ar2.Ar2SetSeed(ar2seeds);

            // Reset CodeBreaker V7 state for input side (1:1 with ResetDevices(INCRYPT))
            if (g_incrypt == Crypt.CRYPT_CB7_COMMON)
                Cb2Crypto.CBSetCommonV7();
            else
                Cb2Crypto.CBReset();


            switch (g_incrypt)
            {
                case Crypt.CRYPT_AR1:
                    Ar2.Ar1BatchDecrypt(cheat);
                    break;

                case Crypt.CRYPT_AR2:
                    Ar2.Ar2BatchDecrypt(cheat);
                    break;

                case Crypt.CRYPT_ARMAX:
				
{
    // 1:1 with armBatchDecryptFull(cheat, armseeds);
    byte ret = Armax.ArmBatchDecryptFull(cheat, armseeds);
    if (ret != 0)
        return ret;

    // If decrypt succeeded, mirror ARMAX metadata (game ID / region)
    // into our globals so the UI and later conversions can use them.
    uint lastGameId = Armax.LastGameId;
    uint lastRegion = Armax.LastRegion;

    // Only overwrite if the values are sane; 0 means "no ID".
    if (lastGameId != 0)
        g_gameid = lastGameId;

    // Regions are 0 = USA, 1 = PAL, 2 = Japan.
    if (lastRegion <= 2)
        g_region = lastRegion;

    break;
}


                case Crypt.CRYPT_CB:
                case Crypt.CRYPT_CB7_COMMON:
                    // 1:1 with CBBatchDecrypt(cheat);
                    Cb2Crypto.CBBatchDecrypt(cheat);
                    break;

                case Crypt.CRYPT_GS3:
                case Crypt.CRYPT_GS5:
                    // 1:1 with gs3BatchDecrypt(cheat, g_gs3key);
                    // Our Gs3.cs exposes BatchDecrypt(cheat) which internally
                    // uses the current GS3 key state.
                    Gs3.BatchDecrypt(cheat);
                    break;

                case Crypt.CRYPT_MAXRAW:
                case Crypt.CRYPT_RAW:
				case Crypt.CRYPT_ARAW:
				case Crypt.CRYPT_CRAW:
				case Crypt.CRYPT_GRAW:
                default:
                    // raw/uncrypted — no-op
                    break;
            }

            return 0;
        }

        /// <summary>
        /// Port of int TranslateCode(cheat_t *cheat) with ARMAX-specific
        /// verifier handling.  The full SCF/device translation logic from
        /// translate.c/scf.c is still TODO.
        /// </summary>
        public static int TranslateCode(cheat_t cheat)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));
            if (cheat.codecnt == 0)
                return 0;

            // --- Strip ARMAX verifier when leaving DEV_ARMAX ---
            if (g_indevice == Device.DEV_ARMAX && g_outdevice != Device.DEV_ARMAX)
            {
                if (g_incrypt == Crypt.CRYPT_ARMAX || g_verifiermode == VerifierMode.MANUAL)
                {
                    // armReadVerifier returns the number of verifier lines.
                    // Each line is 2 octets (2 u32s), so we remove 2 * lines.
                    short lines = Armax.ArmReadVerifier(cheat);
                    if (lines > 0)
                    {
                        int octets = 2 * lines;

                        // The original C removes starting at index 1, so we keep
                        // the first octet and drop the verifier tail.
                        Cheat.cheatRemoveOctets(cheat, 1, octets);
                    }
                }
            }

            // --- Main device->device translation layer (SCF) ---
            if (cheat.codecnt > 1)
            {
                // Port of: if (cheat->codecnt > 1) err = transBatchTranslate(cheat);
                int err = Translate.TransBatchTranslate(cheat);
                if (err != 0)
                    return err;
            }

            // --- Add ARMAX verifier when entering DEV_ARMAX ---
            if (g_outdevice == Device.DEV_ARMAX)
            {
                // Case 1: coming from a non-ARMAX device and we have multiple lines
                if (g_indevice != Device.DEV_ARMAX && cheat.codecnt > 1)
                {
                    uint[] verifier = new uint[2];
                    Armax.ArmMakeVerifier(cheat, verifier, g_gameid, (byte)g_region);

                    // Prepend the two verifier octets (high word last, matches C)
                    Cheat.cheatPrependOctet(cheat, verifier[1]);
                    Cheat.cheatPrependOctet(cheat, verifier[0]);
                }
                // Case 2: auto-verifier mode and input was not already ARMAX
                else if (g_verifiermode == VerifierMode.AUTO &&
                         g_incrypt != Crypt.CRYPT_ARMAX)
                {
                    uint[] verifier = new uint[2];
                    Armax.ArmMakeVerifier(cheat, verifier, g_gameid, (byte)g_region);

                    Cheat.cheatPrependOctet(cheat, verifier[1]);
                    Cheat.cheatPrependOctet(cheat, verifier[0]);
                }
            }

            return 0;
        }

        /// <summary>
        /// Port of int EncryptCode(cheat_t *cheat).
        /// Dispatches based on g_outcrypt.
        /// </summary>
        public static int EncryptCode(cheat_t cheat)
        {
            if (cheat == null) throw new ArgumentNullException(nameof(cheat));
            if (cheat.codecnt == 0)
                return 0;


            // Reset CodeBreaker V7 state for output side (1:1 with ResetDevices(OUTCRYPT))
            if (g_outcrypt == Crypt.CRYPT_CB7_COMMON)
                Cb2Crypto.CBSetCommonV7();
            else
                Cb2Crypto.CBReset();


            switch (g_outcrypt)
            {
                case Crypt.CRYPT_AR1:
                    Ar2.Ar1BatchEncrypt(cheat);
                    break;

                case Crypt.CRYPT_AR2:
                    // 1:1 with:
                    //   ar2AddKeyCode(cheat);
                    //   ar2BatchEncrypt(cheat);
                    Ar2.Ar2AddKeyCode(cheat);
                    Ar2.Ar2BatchEncrypt(cheat);
                    break;

                case Crypt.CRYPT_ARMAX:
                    {
                        // 1:1 with armBatchEncryptFull(cheat, armseeds);
                        byte ret = Armax.ArmBatchEncryptFull(cheat, armseeds);
                        if (ret != 0)
                            return ret;
                        break;
                    }
                case Crypt.CRYPT_CB:
                case Crypt.CRYPT_CB7_COMMON:
                    // 1:1 with CBBatchEncrypt(cheat);
                    Cb2Crypto.CBBatchEncrypt(cheat);
                    break;

                case Crypt.CRYPT_GS3:
                    // 1:1 with gs3BatchEncrypt(cheat, g_gs3key);
                    Gs3.BatchEncrypt(cheat, (byte)g_gs3key);
                    break;

                case Crypt.CRYPT_GS5:
                    // 1:1 with gs3BatchEncrypt + gs3AddVerifier
                    Gs3.BatchEncrypt(cheat, (byte)g_gs3key);
                    Gs3.AddVerifier(cheat);
                    break;

                case Crypt.CRYPT_MAXRAW:
                case Crypt.CRYPT_RAW:
				case Crypt.CRYPT_ARAW:
				case Crypt.CRYPT_CRAW:
				case Crypt.CRYPT_GRAW:
                default:
                    // raw/uncrypted — no-op
                    break;
            }

            return 0;
        }

        /// <summary>
        /// Port of int ConvertCode(cheat_t *cheat).
        /// High-level entry: Decrypt -> Translate -> Encrypt.
        /// </summary>
        public static int ConvertCode(cheat_t cheat)
{
    if (cheat == null) throw new ArgumentNullException(nameof(cheat));

    // Start assuming the cheat is valid; mark as failed only if something goes wrong.
    cheat.state = 0;

    // Mirror original Omniconvert's ProcessText(): reset GS3/GS5
    // encryption tables using the default halfword seeds for each run.
    Gs3.Init();

    int ret;

    // Decryption step (ARMAX encrypted input, GS3, etc.)
    if ((ret = DecryptCode(cheat)) != 0)
    {
        // mark this cheat as bad and bail out
        cheat.state = 1;
        return ret;
    }

    // Opcode/device translation step (AR/GS/CB/etc logic)
    if ((ret = TranslateCode(cheat)) != 0)
    {
        cheat.state = 1;
        return ret;
    }

    // In the Win32 UI, "Make Folders" runs AFTER translate and BEFORE encrypt.
    bool makeFoldersFlag =
        (g_outdevice == Device.DEV_ARMAX) &&
        g_makefolders;

    // If this is a name-only ARMAX cheat and organizers are enabled,
    // convert it into a folder header.
    if (cheat.codecnt == 0 && makeFoldersFlag)
    {
        Armax.ArmMakeFolder(cheat, g_gameid, (byte)g_region);
    }

    // Compute ARMAX disc hash if requested and we're outputting AR MAX.
    uint discHash = 0;

    if (g_outdevice == Device.DEV_ARMAX && g_hashdrive != '\0')
    {
        discHash = Armax.ComputeDiscHash(g_hashdrive);
    }

    // Final ARMAX/GS3/CBC housekeeping (folder bits, etc.)
    Cheat.cheatFinalizeData(cheat, g_outdevice, discHash, makeFoldersFlag);

    // Encryption step (ARMAX encrypted output, GS3, etc.)
    if ((ret = EncryptCode(cheat)) != 0)
    {
        cheat.state = 1;
        return ret;
    }

    return 0;
}


    }
}