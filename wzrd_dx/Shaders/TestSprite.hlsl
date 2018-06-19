struct vertexIn = {
	float3 PosW : POSITION;
	float2 SizeW : SIZE;
}

struct vertexOut = {
	float3 CenterW : POSITION;
	float2 SizeW : SIZE;
}

vertexOut TestVertexShader(vertexIn vIn) {
	vertexOut vOut;

	vOut.CenterW = vIn.PosW;
	vOut.SizeW = vIn.SizeW;

	return vOut;
}
