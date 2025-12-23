using System;
using System.Collections.Generic;
using System.Diagnostics;
using static OmniconvertCS.ConvertCore;

namespace OmniconvertCS
{
        /// <summary>
        /// C# port of the "standard code format" (SCF) translation helpers from translate.c.
        ///
        /// NOTE: Only the high-level batch dispatcher (TransBatchTranslate) and error
        /// helpers are wired at the moment.  The opcode-level translation routines
        /// (TransOther / TransStdToMax / TransMaxToStd) are stubbed and still need a
        /// full mechanical port from the original C if you want full device-to-device
        /// conversion identical to Omniconvert.
        /// </summary>
    internal static partial class Translate
    {

        /// <summary>
        /// High‑level dispatcher equivalent to int transBatchTranslate(cheat_t *src).
        /// It takes the current global g_indevice / g_outdevice and calls the
        /// appropriate per‑opcode translator.
        ///
        /// The translated octets are written into a temporary cheat_t and then
        /// copied back into <paramref name="src"/>.
        /// </summary>
        internal static int TransBatchTranslate(cheat_t src)
        {
            Trace("=== Translate.TransBatchTranslate === indev={0} outdev={1} codecnt={2}",
			g_indevice, g_outdevice, src.codecnt);
			if (src == null) throw new ArgumentNullException(nameof(src));

            // When devices are the same the original simply returns success.
            if (g_indevice == g_outdevice)
{
    Trace("TransBatchTranslate: indev == outdev ({0}), returning early.", g_indevice);
	for (int j = 0; j < src.codecnt; j++)
    Trace($"  src[{j}] = 0x{src.code[j]:X8}");
    return 0;
}

            // Destination container that accumulates translated octets.
            var dest = Cheat.cheatInit();
            int ret = 0;

            if (g_indevice != Device.DEV_ARMAX)
            {
                if (g_outdevice == Device.DEV_ARMAX)
                {
                    // Standard-format -> ARMAX
                    for (int i = 0; i < src.codecnt;)
{
    Trace($"  -> TransMaxToStd enter i={i}");
    ret = TransMaxToStd(dest, src, ref i);
    Trace($"  <- TransMaxToStd exit  i={i} ret={ret} dest.codecnt={dest.codecnt}");

    if (err_suppress == 0 && ret != 0)
        break;
}
                }
                else
                {
                    // Standard-format -> other non-ARMAX device
                    for (int i = 0; i < src.codecnt;)
                    {
                        ret = TransOther(dest, src, ref i);
                        if (err_suppress == 0 && ret != 0)
                            break;
                    }
                }
            }
            else
            {
                // ARMAX -> standard-format
                for (int i = 0; i < src.codecnt;)
                {
                    ret = TransMaxToStd(dest, src, ref i);
                    if (err_suppress == 0 && ret != 0)
                        break;
                }
            }

            // Copy translated octets back into the source cheat.
            src.code.Clear();
            src.code.AddRange(dest.code);

            return ret;
        }
    }
}

