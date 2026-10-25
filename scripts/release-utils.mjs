import path from 'node:path';
import { execFileSync } from 'node:child_process';
export function packageLayout(version, platform, arch) {
  if (!/^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/.test(version)) throw new Error('Invalid package version');
  if (!['win32','darwin','linux'].includes(platform) || !['x64','arm64'].includes(arch)) throw new Error(`Unsupported platform: ${platform}/${arch}`);
  return { name: `SLANG-IDE-${version}-${platform}-${arch}`, extension: platform === 'linux' ? '.tar.gz' : '.zip',
    resources: platform === 'darwin' ? 'SLANG.app/Contents/Resources' : 'resources',
    entry: platform === 'darwin' ? 'SLANG.app' : platform === 'win32' ? 'SLANG.exe' : 'SLANG',
    executable: platform === 'darwin' ? 'SLANG.app/Contents/MacOS/SLANG' : platform === 'win32' ? 'SLANG.exe' : 'SLANG' };
}
export async function archiveDirectory(directory, archive, platform) {
  if (platform === 'win32') {
    const quote = value => "'" + value.replaceAll("'","''") + "'";
    execFileSync('powershell.exe',['-NoProfile','-NonInteractive','-Command',`$ErrorActionPreference = 'Stop'; Compress-Archive -LiteralPath ${quote(directory)} -DestinationPath ${quote(archive)} -CompressionLevel Optimal`],{stdio:'inherit'});
  } else if (platform === 'darwin') {
    execFileSync('ditto',['-c','-k','--sequesterRsrc','--keepParent',directory,archive],{stdio:'inherit'});
  } else execFileSync('tar',['-czf',archive,'-C',path.dirname(directory),path.basename(directory)],{stdio:'inherit'});
}
