// Local resource bridge. The MIT-licensed I3DShapesTool library decodes the
// original GIANTS binary meshes; no replacement buildings or trees are made.
using System;
using System.IO;
using System.Linq;
using System.Text.Json;
using I3DShapesTool.Lib.Model;
using I3DShapesTool.Lib.Container;
using System.Buffers.Binary;

if(args.Length==2 && args[0]=="--cipher-fixtures") {
    var cases=new System.Collections.Generic.List<object>();
    foreach(byte seed in new byte[]{0,80,210,255})foreach(ulong block in new ulong[]{0,1,0xffffffff,0x100000000})foreach(int length in new[]{1,4,63,64,65,128,257,4096}){
        byte[] input=Enumerable.Range(0,length).Select(i=>(byte)((i*37+11)&255)).ToArray(), decoded=(byte[])input.Clone();
        ulong next=new I3DCipher(seed).Process(decoded,block);
        cases.Add(new{seed,block,length,input=Convert.ToHexString(input),output=Convert.ToHexString(decoded),next});
    }
    File.WriteAllText(args[1],JsonSerializer.Serialize(cases));return;
}
if (args.Length != 2) throw new ArgumentException("FS15ShapeBridge INPUT.i3d.shapes OUTPUT.meshes");
var shapes = new ShapesFile();
using (var source = File.OpenRead(args[0])) {
    var header=FileHeader.Read(source);
    if(header.Version!=3)throw new InvalidDataException("This FS15 bridge is validated only for shape container version 3");
    source.Position=0; shapes.Load(source, strict: true);
}
var meshes = shapes.Shapes.ToArray();
using (var output = new BinaryWriter(File.Create(args[1]))) {
    output.Write(new byte[]{(byte)'F',(byte)'S',(byte)'1',(byte)'5',(byte)'M',(byte)'S',(byte)'H',1});
    output.Write(meshes.Length);
    foreach (var mesh in meshes) {
        output.Write(mesh.Id); output.Write(mesh.Positions.Length); output.Write(mesh.Triangles.Length * 3);
        output.Write(mesh.Subsets.Length);
        foreach (var subset in mesh.Subsets) { output.Write(subset.FirstIndex); output.Write(subset.NumIndices); }
        for (int i=0; i<mesh.Positions.Length; ++i) {
            var p=mesh.Positions[i]; var n=mesh.Normals?[i]; var uv=mesh.UVSets[0]?[i];
            output.Write((float)p.X); output.Write((float)p.Y); output.Write((float)p.Z);
            output.Write((float)(n?.X ?? 0)); output.Write((float)(n?.Y ?? 1)); output.Write((float)(n?.Z ?? 0));
            output.Write((float)(uv?.U ?? 0)); output.Write((float)(uv?.V ?? 0));
        }
        foreach (var triangle in mesh.Triangles) { output.Write(triangle.P1Idx-1); output.Write(triangle.P2Idx-1); output.Write(triangle.P3Idx-1); }
    }
}
Console.WriteLine(JsonSerializer.Serialize(new {
    inputVersion=shapes.Version, inputSeed=shapes.Seed,
    parts=shapes.Parts.Length, meshes=meshes.Length,
    vertices=meshes.Sum(m=>(long)m.Positions.Length), triangles=meshes.Sum(m=>(long)m.Triangles.Length),
    skinned=meshes.Count(m=>m.BlendIndices!=null),
    samples=meshes.Take(8).Select(m=>new {m.Id,m.Name,vertices=m.Positions.Length,triangles=m.Triangles.Length})
},new JsonSerializerOptions{WriteIndented=true}));
