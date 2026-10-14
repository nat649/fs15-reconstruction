// Authored compatibility adapter: apply the upstream cipher to the requested
// byte range only. Modern BinaryReader may use a buffer larger than its read.
using System;
using System.IO;
namespace I3DShapesTool.Lib.Container {
public sealed class CipherStream : Stream {
    readonly Stream source; readonly ICipher cipher; ulong block;
    public CipherStream(Stream input,ICipher decoder){source=input;cipher=decoder;}
    public override bool CanRead=>source.CanRead;
    public override bool CanSeek=>false;
    public override bool CanWrite=>source.CanWrite;
    public override long Length=>source.Length;
    public override long Position{get=>source.Position;set=>source.Position=value;}
    public override void Flush()=>source.Flush();
    public override int Read(byte[] buffer,int offset,int count){
        var data=new byte[count]; int n=0;
        while(n<count){int got=source.Read(data,n,count-n);if(got==0)throw new EndOfStreamException();n+=got;}
        block=cipher.Process(data,block);Array.Copy(data,0,buffer,offset,count);return count;
    }
    public override void Write(byte[] buffer,int offset,int count){var data=new byte[count];Array.Copy(buffer,offset,data,0,count);block=cipher.Process(data,block);source.Write(data,0,count);}
    public override long Seek(long offset,SeekOrigin origin)=>throw new NotSupportedException();
    public override void SetLength(long value)=>throw new NotSupportedException();
}
}
