<h1>OmniconvertCS INFO</h1>
<p class="small-note">
    C# / .NET port of <strong>Omniconvert 1.1.1R</strong>, the classic PS2 cheat code converter by <strong>Pyriel</strong>.
</p>

<!-- Overview -->
<details open>
    <summary>Project Overview</summary>
    <div>
        <p>
            <strong>OmniconvertCS</strong> is a C# port of <strong>Omniconvert 1.1.1R</strong> with additional quality-of-life features
            and helpers for modern systems. It allows converting between multiple PS2 cheat formats
            such as CodeBreaker, Action Replay MAX, GameShark / Xploder, RAW, and PNACH.
        </p>
        <p>
            This project builds on years of PS2 cheat research and tools, particularly the original work by
            <strong>Pyriel</strong> and others in the community.
        </p>
        <div class="callout">
            <p>
                <strong>Original Omniconvert:</strong><br />
                <a href="https://github.com/pyriell/omniconvert" target="_blank" rel="noopener noreferrer">
                    https://github.com/pyriell/omniconvert
                </a>
            </p>
        </div>
    </div>
</details>
<!-- Features -->
<details>
    <summary>Features</summary>
    <div>
        <ul>
            <li>
                <strong>Multi-device PS2 cheat conversion</strong>
                <ul>
                    <li>Convert between RAW, CodeBreaker, Action Replay MAX, GameShark / Xploder, and other supported formats.</li>
                </ul>
            </li>
            <li>
                <strong>ARMAX support</strong>
                <ul>
                    <li>Import and export ARMAX <span class="inline-code">.bin</span> cheat lists.</li>
                    <li>Preserve game names, folder/group names, and code descriptions where possible.</li>
                </ul>
            </li>
            <li>
                <strong>PNACH (RAW) support</strong>
                <ul>
                    <li>Input: parse PNACH files that use <span class="inline-code">[GROUP\Name]</span> headers and
                        <span class="inline-code">patch=1,EE,ADDR,extended,VALUE</span> lines.</li>
                    <li>Output: generate PNACH (RAW) text suitable for PCSX2.</li>
                    <li>Optional <em>Add CRC</em> helper to embed CRC and notes at the top of the PNACH output.</li>
                </ul>
            </li>
            <li>
                <strong>PNACH CRC helper</strong>
                <ul>
                    <li>Drag-and-drop a PS2 ELF to compute the PCSX2 “Game CRC”.</li>
                    <li>Associate CRC + ELF name + Game Name in a simple <span class="inline-code">PnachCRC.json</span> mapping.</li>
                    <li>Use those mappings when generating PNACH text and default <span class="inline-code">.pnach</span> filenames.</li>
                </ul>
            </li>
            <li>
                <strong>ARMAX / P2M tooling</strong>
                <ul>
                    <li>Read and write ARMAX <span class="inline-code">.bin</span> codelists.</li>
                    <li>Load XP/GS P2M data where supported.</li>
                </ul>
            </li>
            <li>
                <strong>Modern Windows build</strong>
                <ul>
                    <li>C# / .NET 8.</li>
                    <li>WinForms UI; easier to tweak, maintain, and extend compared to the original C implementation.</li>
                </ul>
            </li>
        </ul>
    </div>
</details>

<!-- Differences from Original -->
<details>
    <summary>What’s Different From the Original Omniconvert</summary>
    <div>
        <p>
            OmniconvertCS is a <strong>C → C# port</strong> with additional features on top of the original
            <strong>Omniconvert 1.1.1R</strong>:
        </p>
        <ul>
            <li>Ported to <strong>C# / .NET 8</strong> using WinForms.</li>
            <li>Added <strong>PNACH (RAW) input/output</strong> and a PNACH CRC helper UI.</li>
            <li>Introduced <strong>PnachCRC.json</strong> mapping:
                <ul>
                    <li>Lines like <span class="inline-code">SLES_526.41&gt;A1FD63D6&gt;Game Name</span>.</li>
                    <li>Supports multiple ELF names sharing the same CRC.</li>
                </ul>
            </li>
            <li>Quality-of-life improvements:
                <ul>
                    <li><strong>Add CRC</strong> checkbox next to the Output label for PNACH mode.</li>
                    <li><strong>Save As .pnach</strong> with auto-suggested filename:
                        <span class="inline-code">ELF_CRC.pnach</span> (for example, <span class="inline-code">SLUS_203.12_A1FD63D6.pnach</span>).
                    </li>
                    <li>Persistence of certain UI options and settings between runs (depending on your build).</li>
                </ul>
            </li>
            <li>Various UI tweaks and bugfixes around ARMAX / P2M loading, error handling, and formatting.</li>
        </ul>
        <p>
            The <strong>core conversion logic and crypt routines</strong> remain faithful to the original Omniconvert
            and upstream research—this C# port aims to modernize and extend the tool, not rewrite its behavior.
        </p>
    </div>
</details>

<!-- PNACH CRC Helper -->
<details>
    <summary>PNACH CRC Helper (Add CRC &amp; PnachCRC.json)</summary>
    <div>
        <h2>Overview</h2>
        <p>
            The PNACH CRC helper lets you compute PCSX2’s “Game CRC” from a PS2 ELF and associate it with a game name
            and ELF name. These mappings are stored in <span class="inline-code">PnachCRC.json</span> and reused when
            generating PNACH files and default <span class="inline-code">.pnach</span> filenames.
        </p>
        <h2>Using the Helper</h2>
        <ol>
            <li>Set the <strong>Output</strong> format to <strong>Pnach (RAW)</strong>.</li>
            <li>Click the <strong>Add CRC</strong> checkbox next to the Output label.</li>
            <li>
                The <strong>Set PNACH CRC</strong> dialog opens:
                <ul>
                    <li>Drag-and-drop a PS2 ELF onto the dialog, or click <strong>Browse...</strong> and select one
                        (e.g. <span class="inline-code">SLUS_203.12</span>).
                    </li>
                    <li>The helper computes the CRC and tries to match an existing entry in
                        <span class="inline-code">PnachCRC.json</span>.
                    </li>
                    <li>If found, it pre-fills the game name; otherwise, enter a new game name.</li>
                </ul>
            </li>
            <li>
                When you confirm:
                <ul>
                    <li>An entry is saved like <span class="inline-code">SLES_526.41&gt;A1FD63D6&gt;Game Name</span>.</li>
                    <li>Multiple ELF names may share the same CRC; each ELF + CRC + Name combination is preserved.</li>
                </ul>
            </li>
        </ol>
        <h2>Effect on PNACH Output</h2>
        <ul>
            <li>When <strong>Add CRC</strong> is checked and a CRC mapping is present:
                <ul>
                    <li>The PNACH output includes CRC/game name info in the header (comments or structured lines, depending on how you configure it).</li>
                    <li><strong>Save As &rarr; PNACH File (.pnach)</strong> suggests a filename like:
                        <span class="inline-code">ELFNAME_CRC.pnach</span>
                        (e.g. <span class="inline-code">SLUS_203.12_A1FD63D6.pnach</span>).
                    </li>
                </ul>
            </li>
            <li>If <strong>Add CRC</strong> is unchecked, the PNACH filename defaults to <span class="inline-code">PUTNAME.pnach</span>
                and the CRC header is not emitted.</li>
        </ul>
    </div>
</details>

<!-- Download & Usage -->
<details>
    <summary>Download &amp; Basic Usage</summary>
    <div>
        <h2>Download</h2>
        <ol>
            <li>Download the latest release ZIP from the project’s <strong>Releases</strong> page.</li>
            <li>Extract it to a folder, for example:
                <span class="inline-code">C:\Tools\OmniconvertCS</span>.
            </li>
            <li>Run <span class="inline-code">OmniconvertCS.exe</span>.</li>
        </ol>
        <h2>Basic Usage</h2>
        <ol>
            <li>Select an <strong>Input</strong> device/crypt format on the left (e.g. CodeBreaker, ARMAX, RAW).</li>
            <li>Select an <strong>Output</strong> format on the right (e.g. CodeBreaker, ARMAX, PNACH (RAW)).</li>
            <li>Paste or load your cheat codes into the <strong>Input</strong> text area.</li>
            <li>Click <strong>Convert</strong>.</li>
            <li>Copy the result from the <strong>Output</strong> area, or use <strong>File &rarr; Save As</strong>
                to export as:
                <ul>
                    <li>Plain text (<span class="inline-code">.txt</span>)</li>
                    <li>ARMAX <span class="inline-code">.bin</span></li>
                    <li>PNACH <span class="inline-code">.pnach</span></li>
                    <li>Other supported formats</li>
                </ul>
            </li>
        </ol>
        <h2>PNACH (RAW) Input Rules</h2>
        <ul>
            <li>Group/title lines use:
                <span class="inline-code">[GROUP\Name]</span>
            </li>
            <li>Code lines use:
                <span class="inline-code">patch=1,EE,ADDR,extended,VALUE</span>
            </li>
            <li>Other lines such as <span class="inline-code">gametitle=</span>,
                <span class="inline-code">comment=</span>, <span class="inline-code">crc=</span>,
                <span class="inline-code">description=</span> are ignored by the PNACH input parser.
            </li>
        </ul>
    </div>
</details>

<!-- Credits -->
<details>
    <summary>Credits &amp; Acknowledgements</summary>
    <div>
        <h2>Upstream Projects &amp; Authors</h2>
        <ul>
            <li>
                <strong>Pyriel</strong>
                <ul>
                    <li>Original <strong>Omniconvert 1.1.1R</strong> PS2 converter.</li>
                    <li>GS3 / XP4+ crypt implementations.</li>
                    <li>Major reverse-engineering work for device formats and crypto routines.</li>
                    <li>Original repo:
                        <a href="https://github.com/pyriell/omniconvert" target="_blank" rel="noopener noreferrer">
                            https://github.com/pyriell/omniconvert
                        </a>
                    </li>
                </ul>
            </li>
            <li>
                <strong>a-n-t-i-b-a-r-y-o-n</strong>
                <ul>
                    <li>Omniconvert fork with additional references and tweaks.</li>
                    <li>Fork:
                        <a href="https://github.com/a-n-t-i-b-a-r-y-o-n/omniconvert" target="_blank" rel="noopener noreferrer">
                            https://github.com/a-n-t-i-b-a-r-y-o-n/omniconvert
                        </a>
                    </li>
                </ul>
            </li>
            <li>
                <strong>misfire/mlafeldt</strong>
                <ul>
                    <li><strong>CB2Crypt</strong> source.</li>
                    <li>CodeBreaker v2 crypt.</li>
                    <li>Action Replay 2 crypt.</li>
                    <ul>
                    <li><strong>CodeBreaker PS2 File Utility (cb2util)</strong>.</li>
                    <li>Repo:
                        <a href="https://github.com/mlafeldt/cb2util" target="_blank" rel="noopener noreferrer">
                            https://github.com/mlafeldt/cb2util
                        </a>
                    </li>
                </ul>
            </li>
            <li>
                <strong>Parasyte</strong>
                <ul>
                    <li>Action Replay MAX (ARMAX) encryption.</li>
                </ul>
            </li>
            <li>
                <strong>Alexander Valyalkin</strong> (<span class="inline-code">valyala@gmail.com</span>)
                <ul>
                    <li><strong>BIG_INT</strong> arbitrary-precision integer library.</li>
                </ul>
            </li>
    
        </ul>
        <h2>OmniconvertCS (C → C# Conversion)</h2>
        <p>
            <strong>OmniconvertCS</strong> is a C# port of <strong>Omniconvert 1.1.1R</strong> with additional features.
        </p>
        <p>
            This is not meant to take away from anyone who built standalone editors over the years.
            Omniconvert itself was already a huge community effort, pulling together:
        </p>
        <ul>
            <li>Public research on device formats and crypt routines.</li>
            <li>New reversing work to fill in missing pieces.</li>
            <li>A unified tool that made PS2 cheat formats manageable.</li>
        </ul>
        <p>
            <em>
                “Pyriel was always someone I looked up to and is one of the reasons I was even able to do what I do now.
                Converting Omniconvert into C# has been a fun challenge and an honor — I never thought I’d be porting
                something this complex. Big thanks to everyone who contributed to the PS2 scene back in the day and
                keeps it alive now.”
            </em>
        </p>
    </div>
</details>

<!-- Links -->
<details>
    <summary>Useful Links</summary>
    <div>
        <ul>
            <li>
                Original Omniconvert 1.1.1R (PS2):
                <a href="https://github.com/pyriell/omniconvert" target="_blank" rel="noopener noreferrer">
                    https://github.com/pyriell/omniconvert
                </a>
            </li>
            <li>
                Omniconvert fork (a-n-t-i-b-a-r-y-o-n):
                <a href="https://github.com/a-n-t-i-b-a-r-y-o-n/omniconvert" target="_blank" rel="noopener noreferrer">
                    https://github.com/a-n-t-i-b-a-r-y-o-n/omniconvert
                </a>
            </li>
            <li>
                CodeBreaker PS2 File Utility (cb2util):
                <a href="https://github.com/mlafeldt/cb2util" target="_blank" rel="noopener noreferrer">
                    https://github.com/mlafeldt/cb2util
                </a>
            </li>
        </ul>
    </div>
</details>

</body>
</html>
