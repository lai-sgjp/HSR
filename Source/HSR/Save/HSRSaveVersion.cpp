#include "HSRSaveVersion.h"
#include "../Challenge/HSRChallengeProgressionSubsystem.h"
#include "Misc/DateTime.h"
#include <type_traits>

// 匿名命名空间：这套辅助代码只在本编译单元可见。
// 这里实现了存档的「规范二进制编码」（canonical payload）所需的全部读写原语，
// 以及 SHA-256 校验和计算。核心设计原则：
//   - 所有名称（FName）都按「小写规范形式」编码（CanonicalName），并强制限定字符集，
//     保证同一逻辑名称在不同平台/大小写下的编码结果一致；
//   - 所有可变长数组都带长度前缀，并受 MaxRecordCount 上限约束，防止恶意/损坏数据
//     撑爆内存；
//   - 浮点以 IEEE754 位模式编码（不经过文本），保证精确往返；
//   - 数组一律要求「按编码规则排序后再编码」，解码端用同样的排序规则校验，
//     从而让「同一份逻辑数据」只能有唯一的字节表示（可复现/可校验）。
namespace
{
constexpr uint32 MaxCount = HSRSaveVersion::MaxRecordCount;

// 把 FName 规约成规范 ID 字符串：空名 -> 空串，否则转小写。
FString CanonicalName(const FName& Value)
{
	return Value.IsNone() ? FString() : Value.ToString().ToLower();
}

// 按规范名称做字典序比较（用于稳定排序）。
bool NameLess(const FName& A, const FName& B)
{
	return CanonicalName(A).Compare(CanonicalName(B), ESearchCase::CaseSensitive) < 0;
}

// 32 位循环右移（SHA-256 需要）。
uint32 RotateRight(const uint32 Value, const uint32 Count)
{
	return (Value >> Count) | (Value << (32 - Count));
}

// 纯 C++ 的 SHA-256 实现：输出 32 字节摘要。
// 这是自包含实现，不依赖第三方库；用于给存档信封计算校验和，
// 保证磁盘上的字节与写入时一致（损坏/篡改都能被发现）。
void ComputeSha256(const uint8* Data, const int64 Size, uint8 OutDigest[32])
{
	// SHA-256 的 64 个轮常量（π 小数部分前 32 位）。
	static constexpr uint32 K[64] = {
		0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
		0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
		0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
		0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
		0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
		0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
		0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
		0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
	// 初始哈希值（前 8 个素数平方根的小数部分）。
	uint32 H[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};

	// 填充：追加 0x80 位，再补零到 56 字节对齐，最后 8 字节写原始位长（大端）。
	TArray<uint8> Message;
	Message.Append(Data, Size);
	Message.Add(0x80);
	while ((Message.Num() % 64) != 56)
	{
		Message.Add(0);
	}
	const uint64 BitSize = static_cast<uint64>(Size) * 8;
	for (int32 Shift = 56; Shift >= 0; Shift -= 8)
	{
		Message.Add(static_cast<uint8>(BitSize >> Shift));
	}

	// 逐 64 字节块处理。
	for (int32 Offset = 0; Offset < Message.Num(); Offset += 64)
	{
		// 消息调度：前 16 个字直接取自块（大端），后 48 个字由前字推导。
		uint32 W[64];
		for (int32 I = 0; I < 16; ++I)
		{
			W[I] = (uint32(Message[Offset + I * 4]) << 24)
				| (uint32(Message[Offset + I * 4 + 1]) << 16)
				| (uint32(Message[Offset + I * 4 + 2]) << 8)
				| Message[Offset + I * 4 + 3];
		}
		for (int32 I = 16; I < 64; ++I)
		{
			const uint32 S0 = RotateRight(W[I - 15], 7) ^ RotateRight(W[I - 15], 18) ^ (W[I - 15] >> 3);
			const uint32 S1 = RotateRight(W[I - 2], 17) ^ RotateRight(W[I - 2], 19) ^ (W[I - 2] >> 10);
			W[I] = W[I - 16] + S0 + W[I - 7] + S1;
		}

		// 压缩主循环：8 个工作变量迭代 64 轮。
		uint32 A = H[0], B = H[1], C = H[2], D = H[3], E = H[4], F = H[5], G = H[6], HH = H[7];
		for (int32 I = 0; I < 64; ++I)
		{
			const uint32 S1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
			const uint32 Ch = (E & F) ^ ((~E) & G);
			const uint32 T1 = HH + S1 + Ch + K[I] + W[I];
			const uint32 S0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
			const uint32 Maj = (A & B) ^ (A & C) ^ (B & C);
			const uint32 T2 = S0 + Maj;
			HH = G;
			G = F;
			F = E;
			E = D + T1;
			D = C;
			C = B;
			B = A;
			A = T1 + T2;
		}

		// 块处理后累加回初始值。
		H[0] += A;
		H[1] += B;
		H[2] += C;
		H[3] += D;
		H[4] += E;
		H[5] += F;
		H[6] += G;
		H[7] += HH;
	}

	// 输出：按大端逐字写回摘要。
	for (int32 I = 0; I < 8; ++I)
	{
		for (int32 J = 0; J < 4; ++J)
		{
			OutDigest[I * 4 + J] = static_cast<uint8>(H[I] >> (24 - J * 8));
		}
	}
}

// 写入端：把各类型追加到字节流末尾。
struct FWriter
{
	TArray<uint8>& B;
	explicit FWriter(TArray<uint8>& In) : B(In) {}

	void U8(uint8 V)
	{
		B.Add(V);
	}

	// 任意整数以小端写入（LE 序列化是信封的自定约定）。
	template<typename T>
	void I(T V)
	{
		using U = std::make_unsigned_t<T>;
		for (int32 I = 0; I < sizeof(T); ++I)
		{
			U8(static_cast<uint8>((static_cast<U>(V) >> (I * 8)) & 0xff));
		}
	}

	void Guid(const FGuid& V)
	{
		I<uint32>(V.A);
		I<uint32>(V.B);
		I<uint32>(V.C);
		I<uint32>(V.D);
	}

	// 名称写入：规范小写形式 + UTF-8 + 长度前缀。返回 false 表示该名称无法被
	// 规范表示（例如包含非法字符），调用方应据此拒绝整条记录。
	bool Name(const FName& V)
	{
		FString S = CanonicalName(V);
		if (!HSRSaveVersion::IsCanonicalIdToken(S))
		{
			return false;
		}
		FTCHARToUTF8 Utf8(*S);
		I<uint32>(Utf8.Length());
		B.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		return true;
	}

	// 必填名称：空名不允许。
	bool NameRequired(const FName& V)
	{
		return !V.IsNone() && Name(V);
	}

	// double 以 IEEE754 位模式写入；非有限值拒绝（0.0 归一化掉负零）。
	bool D(double V)
	{
		if (!FMath::IsFinite(V))
		{
			return false;
		}
		if (V == 0.0)
		{
			V = 0.0;
		}
		uint64 Bits;
		FMemory::Memcpy(&Bits, &V, sizeof(Bits));
		I<uint64>(Bits);
		return true;
	}
};

// 读取端：与 FWriter 对称地从字节流消费数据。bOk 置 false 表示发生越界/非法数据。
struct FReader
{
	const TArray<uint8>& B;
	int64 P = 0;
	bool bOk = true;

	explicit FReader(const TArray<uint8>& In) : B(In) {}

	uint8 U8()
	{
		return P < B.Num() ? B[P++] : (bOk = false, 0);
	}

	// 读任意整数（小端）。
	template<typename T>
	T I()
	{
		using U = std::make_unsigned_t<T>;
		U V = 0;
		for (int32 X = 0; X < sizeof(T); ++X)
		{
			V |= static_cast<U>(U8()) << (X * 8);
		}
		return static_cast<T>(V);
	}

	FGuid Guid()
	{
		const uint32 A = I<uint32>();
		const uint32 C1 = I<uint32>();
		const uint32 C2 = I<uint32>();
		const uint32 D = I<uint32>();
		return FGuid(A, C1, C2, D);
	}

	// 读名称：先读长度，再校验长度/越界/字符集/大小写规范，全部通过才回填 Out。
	bool Name(FName& Out)
	{
		const uint32 N = I<uint32>();
		if (!bOk || N > HSRSaveVersion::MaxTokenBytes || P + N > B.Num())
		{
			return bOk = false;
		}
		FString S;
		FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(B.GetData() + P), N);
		S = FString(Conv.Length(), Conv.Get());
		P += N;
		if (!S.IsEmpty())
		{
			for (TCHAR C : S)
			{
				if (!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '.' || C == '_' || C == '-'))
				{
					return bOk = false;
				}
			}
			// 规范形式要求：读到的字符串必须已经是小写规范形式，否则视为非规范数据。
			if (FName(*S).ToString().ToLower() != S)
			{
				return bOk = false;
			}
			Out = FName(*S);
		}
		else
		{
			Out = NAME_None;
		}
		return true;
	}

	bool NameRequired(FName& Out)
	{
		return Name(Out) && !Out.IsNone();
	}

	// 读 double（IEEE754 位模式）；非有限值视为数据损坏。
	bool D(double& Out)
	{
		const uint64 Bits = I<uint64>();
		FMemory::Memcpy(&Out, &Bits, sizeof(Bits));
		if (!FMath::IsFinite(Out))
		{
			return bOk = false;
		}
		if (Out == 0.0)
		{
			Out = 0.0;
		}
		return bOk;
	}
};

// 写入数组长度前缀；超过上限直接失败。
template<typename T>
bool Count(FWriter& W, const TArray<T>& A)
{
	if (static_cast<uint64>(A.Num()) > HSRSaveVersion::MaxRecordCount)
	{
		return false;
	}
	W.I<uint32>(A.Num());
	return true;
}

// 读取数组长度前缀并预分配；长度非法则失败。
template<typename T>
bool ReadCount(FReader& R, TArray<T>& A)
{
	const uint32 N = R.I<uint32>();
	if (!R.bOk || N > HSRSaveVersion::MaxRecordCount)
	{
		return R.bOk = false;
	}
	A.SetNum(N);
	return true;
}

// FTransform 编码：四元数（旋转）+ 位置 + 缩放，共 10 个 double。
bool Transform(FWriter& W, const FTransform& T)
{
	const FQuat Q = T.GetRotation();
	const FVector L = T.GetLocation(), S = T.GetScale3D();
	return W.D(Q.X) && W.D(Q.Y) && W.D(Q.Z) && W.D(Q.W)
		&& W.D(L.X) && W.D(L.Y) && W.D(L.Z)
		&& W.D(S.X) && W.D(S.Y) && W.D(S.Z);
}

bool Transform(FReader& R, FTransform& T)
{
	double X, Y, Z, W, A, B, C, D, E, F;
	if (!R.D(X) || !R.D(Y) || !R.D(Z) || !R.D(W)
		|| !R.D(A) || !R.D(B) || !R.D(C) || !R.D(D) || !R.D(E) || !R.D(F))
	{
		return false;
	}
	T = FTransform(FQuat(X, Y, Z, W), FVector(A, B, C), FVector(D, E, F));
	return true;
}

// 角色档案 DTO 编码：CharacterId + 等级/经验/突破 + 技能等级表 + 运行时版本号。
bool Profile(FWriter& W, const FHSRSaveProfileDto& P)
{
	if (!W.NameRequired(P.State.CharacterId))
	{
		return false;
	}
	W.I<int32>(P.State.Level);
	W.I<int32>(P.State.Experience);
	W.I<int32>(P.State.Ascension);
	// 技能等级表按键排序后编码，保证字节可复现。
	TArray<FName> K;
	P.State.SkillLevels.GetKeys(K);
	K.Sort(NameLess);
	if (!Count(W, K))
	{
		return false;
	}
	for (FName N : K)
	{
		if (!W.NameRequired(N))
		{
			return false;
		}
		W.I<int32>(P.State.SkillLevels[N]);
	}
	W.I<int64>(P.RuntimeRevision);
	return true;
}

// 角色档案 DTO 解码：与编码对称，并校验技能键严格递增（非规范则拒绝）。
bool Profile(FReader& R, FHSRSaveProfileDto& P)
{
	if (!R.NameRequired(P.State.CharacterId))
	{
		return false;
	}
	P.State.Level = R.I<int32>();
	P.State.Experience = R.I<int32>();
	P.State.Ascension = R.I<int32>();
	uint32 N = R.I<uint32>();
	if (!R.bOk || N > HSRSaveVersion::MaxRecordCount)
	{
		return R.bOk = false;
	}
	FName Prev;
	bool bHasPrev = false;
	for (uint32 I = 0; I < N; ++I)
	{
		FName K;
		if (!R.NameRequired(K) || (bHasPrev && !NameLess(Prev, K)))
		{
			return R.bOk = false;
		}
		Prev = K;
		bHasPrev = true;
		P.State.SkillLevels.Add(K, R.I<int32>());
	}
	P.RuntimeRevision = R.I<int64>();
	return R.bOk;
}

// 地图位置编码：MapId + 到达点 + 世界变换。
bool Location(FWriter& W, const FHSRMapLocation& L)
{
	return W.Name(L.MapId) && W.Name(L.ArrivalId) && Transform(W, L.WorldTransform);
}

bool Location(FReader& R, FHSRMapLocation& L)
{
	return R.Name(L.MapId) && R.Name(L.ArrivalId) && Transform(R, L.WorldTransform);
}

// 名称数组编码：先排序去重，再逐个编码；重复/空名都会导致失败。
bool Names(FWriter& W, TArray<FName> A)
{
	A.Sort(NameLess);
	for (int32 I = 1; I < A.Num(); ++I)
	{
		if (CanonicalName(A[I - 1]) == CanonicalName(A[I]))
		{
			return false;
		}
	}
	if (!Count(W, A))
	{
		return false;
	}
	for (FName N : A)
	{
		if (!W.NameRequired(N))
		{
			return false;
		}
	}
	return true;
}

// 名称数组解码：要求严格递增。
bool Names(FReader& R, TArray<FName>& A)
{
	if (!ReadCount(R, A))
	{
		return false;
	}
	FName P;
	bool bHasPrev = false;
	for (FName& N : A)
	{
		if (!R.NameRequired(N) || (bHasPrev && !NameLess(P, N)))
		{
			return R.bOk = false;
		}
		P = N;
		bHasPrev = true;
	}
	return true;
}

// 背包存档编码：堆叠物品（按 ItemId 排序）+ 唯一物品（按 InstanceId 排序）+ 版本号。
bool Inventory(FWriter& W, const FHSRInventorySaveData& D)
{
	auto S = D.Stacks;
	S.Sort([](const auto& A, const auto& B) { return NameLess(A.ItemId, B.ItemId); });
	for (int32 I = 1; I < S.Num(); ++I)
	{
		if (CanonicalName(S[I - 1].ItemId) == CanonicalName(S[I].ItemId))
		{
			return false;
		}
	}
	if (!Count(W, S))
	{
		return false;
	}
	for (const auto& X : S)
	{
		if (!W.NameRequired(X.ItemId))
		{
			return false;
		}
		W.I<int32>(X.Quantity);
	}
	auto U = D.UniqueItems;
	U.Sort([](const auto& A, const auto& B) { return A.InstanceId < B.InstanceId; });
	for (int32 I = 1; I < U.Num(); ++I)
	{
		if (U[I - 1].InstanceId == U[I].InstanceId)
		{
			return false;
		}
	}
	if (!Count(W, U))
	{
		return false;
	}
	for (const auto& X : U)
	{
		if (!X.InstanceId.IsValid())
		{
			return false;
		}
		W.Guid(X.InstanceId);
		if (!W.NameRequired(X.DefinitionId))
		{
			return false;
		}
	}
	W.I<int64>(D.Revision);
	return true;
}

// 背包存档解码：对称校验（堆叠按 ItemId 递增、唯一物品按 InstanceId 递增）。
bool Inventory(FReader& R, FHSRInventorySaveData& D)
{
	if (!ReadCount(R, D.Stacks))
	{
		return false;
	}
	FName P;
	bool bHasPrev = false;
	for (auto& X : D.Stacks)
	{
		if (!R.NameRequired(X.ItemId) || (bHasPrev && !NameLess(P, X.ItemId)))
		{
			return R.bOk = false;
		}
		P = X.ItemId;
		bHasPrev = true;
		X.Quantity = R.I<int32>();
	}
	if (!ReadCount(R, D.UniqueItems))
	{
		return false;
	}
	FGuid G;
	for (int32 I = 0; I < D.UniqueItems.Num(); ++I)
	{
		auto& X = D.UniqueItems[I];
		X.InstanceId = R.Guid();
		if (!X.InstanceId.IsValid() || (I && !(G < X.InstanceId)))
		{
			return R.bOk = false;
		}
		G = X.InstanceId;
		if (!R.NameRequired(X.DefinitionId))
		{
			return false;
		}
	}
	D.Revision = R.I<int64>();
	return R.bOk;
}

// 奖励存档编码：收据数组（按 ClaimId 排序）+ 每张收据内的发放（按 ItemId 排序）。
bool Reward(FWriter& W, const FHSRRewardSaveData& D)
{
	auto A = D.Receipts;
	A.Sort([](const auto& X, const auto& Y) { return X.Request.ClaimId < Y.Request.ClaimId; });
	for (int32 I = 1; I < A.Num(); ++I)
	{
		if (A[I - 1].Request.ClaimId == A[I].Request.ClaimId)
		{
			return false;
		}
	}
	if (!Count(W, A))
	{
		return false;
	}
	for (auto X : A)
	{
		if (!X.Request.ClaimId.IsValid())
		{
			return false;
		}
		W.Guid(X.Request.ClaimId);
		if (!W.NameRequired(X.Request.RewardDefinitionId))
		{
			return false;
		}
		W.I<int32>(X.Request.Seed);
		// 每张收据内的物品发放（Grants）同样排序去重。
		auto G = X.Grants;
		G.Sort([](const auto& L, const auto& R) { return NameLess(L.ItemId, R.ItemId); });
		for (int32 I = 1; I < G.Num(); ++I)
		{
			if (CanonicalName(G[I - 1].ItemId) == CanonicalName(G[I].ItemId))
			{
				return false;
			}
		}
		if (!Count(W, G))
		{
			return false;
		}
		for (auto& V : G)
		{
			if (!W.NameRequired(V.ItemId))
			{
				return false;
			}
			W.I<int32>(V.Quantity);
			// 唯一物品实例 ID 列表：排序 + 去重后编码。
			V.InstanceIds.Sort();
			for (int32 I = 0; I < V.InstanceIds.Num(); ++I)
			{
				if (!V.InstanceIds[I].IsValid() || (I && V.InstanceIds[I - 1] == V.InstanceIds[I]))
				{
					return false;
				}
			}
			if (!Count(W, V.InstanceIds))
			{
				return false;
			}
			for (const FGuid& I : V.InstanceIds)
			{
				W.Guid(I);
			}
		}
		W.I<int64>(X.Revision);
	}
	W.I<int64>(D.Revision);
	return true;
}

// 奖励存档解码：对称校验（收据按 ClaimId、发放按 ItemId、实例 ID 列表递增）。
bool Reward(FReader& R, FHSRRewardSaveData& D)
{
	if (!ReadCount(R, D.Receipts))
	{
		return false;
	}
	FGuid P;
	for (int32 I = 0; I < D.Receipts.Num(); ++I)
	{
		auto& X = D.Receipts[I];
		X.Request.ClaimId = R.Guid();
		if (!X.Request.ClaimId.IsValid() || (I && !(P < X.Request.ClaimId)))
		{
			return R.bOk = false;
		}
		P = X.Request.ClaimId;
		if (!R.NameRequired(X.Request.RewardDefinitionId))
		{
			return false;
		}
		X.Request.Seed = R.I<int32>();
		if (!ReadCount(R, X.Grants))
		{
			return false;
		}
		FName PN;
		bool bHasPrev = false;
		for (auto& V : X.Grants)
		{
			if (!R.NameRequired(V.ItemId) || (bHasPrev && !NameLess(PN, V.ItemId)))
			{
				return R.bOk = false;
			}
			PN = V.ItemId;
			bHasPrev = true;
			V.Quantity = R.I<int32>();
			if (!ReadCount(R, V.InstanceIds))
			{
				return false;
			}
			FGuid PG;
			for (int32 J = 0; J < V.InstanceIds.Num(); ++J)
			{
				V.InstanceIds[J] = R.Guid();
				if (!V.InstanceIds[J].IsValid() || (J && !(PG < V.InstanceIds[J])))
				{
					return R.bOk = false;
				}
				PG = V.InstanceIds[J];
			}
		}
		X.Revision = R.I<int64>();
	}
	D.Revision = R.I<int64>();
	return R.bOk;
}

// 任务存档编码：状态数组（按 QuestId 排序），内含目标数组与领取信息。
bool Quest(FWriter& W, const FHSRQuestSaveData& D)
{
	auto A = D.States;
	A.Sort([](const auto& X, const auto& Y) { return NameLess(X.QuestId, Y.QuestId); });
	for (int32 I = 1; I < A.Num(); ++I)
	{
		if (CanonicalName(A[I - 1].QuestId) == CanonicalName(A[I].QuestId))
		{
			return false;
		}
	}
	if (!Count(W, A))
	{
		return false;
	}
	for (auto X : A)
	{
		if (!W.NameRequired(X.QuestId) || static_cast<uint8>(X.State) > static_cast<uint8>(EHSRQuestState::Completed))
		{
			return false;
		}
		W.U8(uint8(X.State));
		auto O = X.Objectives;
		O.Sort([](const auto& L, const auto& R) { return NameLess(L.ObjectiveId, R.ObjectiveId); });
		for (int32 I = 1; I < O.Num(); ++I)
		{
			if (CanonicalName(O[I - 1].ObjectiveId) == CanonicalName(O[I].ObjectiveId))
			{
				return false;
			}
		}
		if (!Count(W, O))
		{
			return false;
		}
		for (const auto& V : O)
		{
			if (!W.NameRequired(V.ObjectiveId))
			{
				return false;
			}
			W.I<int32>(V.CurrentCount);
			W.I<int32>(V.RequiredCount);
			W.U8(V.bCompleted ? 1 : 0);
		}
		W.U8(X.bRewardClaimed ? 1 : 0);
		W.Guid(X.RewardClaimId);
		W.I<int64>(X.Revision);
	}
	W.I<int64>(D.Revision);
	return true;
}

// 任务存档解码：对称校验。
bool Quest(FReader& R, FHSRQuestSaveData& D)
{
	if (!ReadCount(R, D.States))
	{
		return false;
	}
	FName P;
	bool bHasPrev = false;
	for (auto& X : D.States)
	{
		if (!R.NameRequired(X.QuestId) || (bHasPrev && !NameLess(P, X.QuestId)))
		{
			return R.bOk = false;
		}
		P = X.QuestId;
		bHasPrev = true;
		uint8 S = R.U8();
		if (S > uint8(EHSRQuestState::Completed))
		{
			return R.bOk = false;
		}
		X.State = EHSRQuestState(S);
		if (!ReadCount(R, X.Objectives))
		{
			return false;
		}
		FName PO;
		bool bHasObjective = false;
		for (auto& V : X.Objectives)
		{
			if (!R.NameRequired(V.ObjectiveId) || (bHasObjective && !NameLess(PO, V.ObjectiveId)))
			{
				return R.bOk = false;
			}
			PO = V.ObjectiveId;
			bHasObjective = true;
			V.CurrentCount = R.I<int32>();
			V.RequiredCount = R.I<int32>();
			uint8 C = R.U8();
			if (C > 1)
			{
				return R.bOk = false;
			}
			V.bCompleted = C != 0;
		}
		uint8 C = R.U8();
		if (C > 1)
		{
			return R.bOk = false;
		}
		X.bRewardClaimed = C != 0;
		X.RewardClaimId = R.Guid();
		X.Revision = R.I<int64>();
	}
	D.Revision = R.I<int64>();
	return R.bOk;
}

// 挑战进度存档编码：已通关遭遇 ID 数组 + 版本号。
bool ChallengeProgression(FWriter& W, const FHSRChallengeProgressionSaveData& D)
{
	if (D.Revision < 0)
	{
		return false;
	}
	auto A = D.CompletedEncounterIds;
	A.Sort(NameLess);
	for (int32 I = 1; I < A.Num(); ++I)
	{
		if (CanonicalName(A[I - 1]) == CanonicalName(A[I]))
		{
			return false;
		}
	}
	if (!Count(W, A))
	{
		return false;
	}
	for (const FName Id : A)
	{
		if (!W.NameRequired(Id))
		{
			return false;
		}
	}
	W.I<int64>(D.Revision);
	return true;
}

// 挑战进度存档解码。
bool ChallengeProgression(FReader& R, FHSRChallengeProgressionSaveData& D)
{
	if (!ReadCount(R, D.CompletedEncounterIds))
	{
		return false;
	}
	FName P;
	bool bHasPrev = false;
	for (FName& Id : D.CompletedEncounterIds)
	{
		if (!R.NameRequired(Id) || (bHasPrev && !NameLess(P, Id)))
		{
			return R.bOk = false;
		}
		P = Id;
		bHasPrev = true;
	}
	D.Revision = R.I<int64>();
	return R.bOk && D.Revision >= 0;
}
}

// 槽位名校验：非空、不含冒号、用户索引非负。冒号被保留用于构造槽位身份串。
bool HSRSaveVersion::IsValidSlot(const FString& SlotName, int32 User)
{
	return !SlotName.IsEmpty() && !SlotName.Contains(TEXT(":")) && User >= 0;
}

// 载荷大小必须落在信封头声明的上限内（防止恶意声明撑爆读取）。
bool HSRSaveVersion::IsValidPayloadSize(uint64 PayloadBytes)
{
	return PayloadBytes <= static_cast<uint64>(FHSRSaveEnvelopeHeader::MaxPayloadBytes);
}

// 规范 ID 词法：仅允许小写字母/数字/./_/-，且长度不超过 MaxTokenBytes。
// 这是 FName 编码前必须满足的约束，保证同一逻辑名称的字节表示唯一且稳定。
bool HSRSaveVersion::IsCanonicalIdToken(const FString& Token)
{
	FTCHARToUTF8 Utf8(*Token);
	if (Utf8.Length() > MaxTokenBytes)
	{
		return false;
	}
	for (TCHAR C : Token)
	{
		if (!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '.' || C == '_' || C == '-'))
		{
			return false;
		}
	}
	return true;
}

// 槽位身份：由「槽位名:用户索引」字符串经 FNV-1a 64 位哈希得到。
// 写入信封头，读取时比对，防止把 A 槽位的文件内容装到 B 槽位名下。
uint64 HSRSaveVersion::MakeSlotIdentity(const FString& Slot, int32 User)
{
	FTCHARToUTF8 S(*(Slot + TEXT(":") + LexToString(User)));
	uint64 H = 14695981039346656037ull; // FNV-1a 64 位偏移基
	for (int32 I = 0; I < S.Length(); ++I)
	{
		H ^= static_cast<uint8>(S.Get()[I]);
		H *= 1099511628211ull;
	}
	return H;
}

// 编码规范的 payload（不含信封头）：所有域按固定顺序、固定字段序写入。
// 这是整个存档可复现性的核心：同一个逻辑数据只能有一种字节表示。
bool HSRSaveVersion::EncodeCanonicalPayload(const FHSRSaveData& D, TArray<uint8>& O)
{
	// Schema >= 7 时旧的扁平装备数组必须为空（数据已迁移到 registry/placement）。
	if (D.SchemaVersion >= 7 && !D.Equipment.IsEmpty())
	{
		return false;
	}

	O.Reset();
	FWriter W(O);
	W.I<uint32>(D.SchemaVersion);

	// 角色档案：先排序后编码，解码端用同样的排序校验。
	TArray<FHSRSaveProfileDto> P = D.Profiles;
	P.Sort([](const auto& A, const auto& B)
	{
		return NameLess(A.State.CharacterId, B.State.CharacterId);
	});
	for (int32 I = 1; I < P.Num(); ++I)
	{
		if (CanonicalName(P[I - 1].State.CharacterId) == CanonicalName(P[I].State.CharacterId))
		{
			return false;
		}
	}
	if (!Count(W, P))
	{
		return false;
	}
	for (const auto& X : P)
	{
		if (!Profile(W, X))
		{
			return false;
		}
	}

	// 队伍槽位：数量必须与该 schema 声明的宽度一致；槽位内容允许空名。
	if (D.PartySlots.Num() != static_cast<int32>(PartySlotCountForSchema(D.SchemaVersion)) || !Count(W, D.PartySlots))
	{
		return false;
	}
	for (const auto& X : D.PartySlots)
	{
		if (!W.Name(X.CharacterId))
		{
			return false;
		}
	}
	W.I<int64>(D.PartyRevision);

	// 其余域以稳定的、字段有序的记录编码。
	if (D.SchemaVersion >= 7)
	{
		// 装备注册表（registry）：每件实例按 InstanceId 排序。
		auto E = D.EquipmentRegistry;
		E.Sort([](const auto& A, const auto& B) { return A.InstanceId < B.InstanceId; });
		for (int32 I = 1; I < E.Num(); ++I)
		{
			if (E[I - 1].InstanceId == E[I].InstanceId)
			{
				return false;
			}
		}
		if (!Count(W, E))
		{
			return false;
		}
		for (const auto& X : E)
		{
			if (!X.InstanceId.IsValid() || X.Kind < 0 || X.Kind > 1 || !W.NameRequired(X.DefinitionId))
			{
				return false;
			}
			W.Guid(X.InstanceId);
			W.I<int32>(X.Kind);
			W.I<int32>(X.EnhancementLevel);
			if (!Count(W, X.Modifiers))
			{
				return false;
			}
			for (const auto& M : X.Modifiers)
			{
				if (static_cast<uint8>(M.Stat) > static_cast<uint8>(EHSREquipmentStat::Speed))
				{
					return false;
				}
				W.U8(static_cast<uint8>(M.Stat));
				if (!W.D(M.Value))
				{
					return false;
				}
			}
			if (!W.Name(X.SetId))
			{
				return false;
			}
		}
		// 装备摆放（placement）：按 角色 GUID -> 种类 -> 槽位 -> 实例 ID 排序。
		auto L = D.EquipmentPlacements;
		L.Sort([](const auto& A, const auto& B)
		{
			if (A.CharacterId != B.CharacterId)
			{
				return A.CharacterId < B.CharacterId;
			}
			if (A.Kind != B.Kind)
			{
				return A.Kind < B.Kind;
			}
			if (A.Slot != B.Slot)
			{
				return A.Slot < B.Slot;
			}
			return A.InstanceId < B.InstanceId;
		});
		if (!Count(W, L))
		{
			return false;
		}
		for (const auto& X : L)
		{
			if (!X.InstanceId.IsValid() || !X.CharacterId.IsValid() || X.Kind < 0 || X.Kind > 1 || X.Slot < 0)
			{
				return false;
			}
			W.Guid(X.InstanceId);
			W.Guid(X.CharacterId);
			W.I<int32>(X.Kind);
			W.I<int32>(X.Slot);
			W.I<int32>(X.AuthorityRevision);
		}
	}
	else
	{
		// 旧 schema（<7）的扁平装备数组。
		TArray<FHSREquipmentSaveDto> E = D.Equipment;
		E.Sort([](const auto& A, const auto& B) { return A.InstanceId < B.InstanceId; });
		for (int32 I = 1; I < E.Num(); ++I)
		{
			if (E[I - 1].InstanceId == E[I].InstanceId)
			{
				return false;
			}
		}
		if (!Count(W, E))
		{
			return false;
		}
		for (const auto& X : E)
		{
			if (!X.InstanceId.IsValid() || !X.CharacterId.IsValid() || X.Kind < 0 || X.Kind > 1
				|| X.Slot < 0
				|| (X.Kind == 0 && X.Slot > static_cast<int32>(EHSREquipmentSlot::Feet))
				|| (X.Kind == 1 && X.Slot > static_cast<int32>(EHSRRelicSlot::LinkRope))
				|| !W.NameRequired(X.DefinitionId))
			{
				return false;
			}
			W.Guid(X.InstanceId);
			W.Guid(X.CharacterId);
			W.I<int32>(X.Kind);
			W.I<int32>(X.Slot);
			W.I<int32>(X.EnhancementLevel);
			if (!Count(W, X.Modifiers))
			{
				return false;
			}
			for (const auto& M : X.Modifiers)
			{
				if (static_cast<uint8>(M.Stat) > static_cast<uint8>(EHSREquipmentStat::Speed))
				{
					return false;
				}
				W.I<uint8>(static_cast<uint8>(M.Stat));
				if (!W.D(M.Value))
				{
					return false;
				}
			}
			if (!W.Name(X.SetId))
			{
				return false;
			}
			W.I<int32>(X.AuthorityRevision);
		}
	}

	// 背包、奖励、任务三个域：固定顺序。
	if (!Inventory(W, D.Inventory) || !Reward(W, D.Rewards) || !Quest(W, D.Quests))
	{
		return false;
	}

	// 地图域：区域/传送点/探索标记名称数组 + 当前位置 + 版本号。
	if (!Names(W, D.Map.UnlockedRegionIds) || !Names(W, D.Map.UnlockedTeleportIds)
		|| !Names(W, D.Map.ExplorationFlags) || !Location(W, D.Map.CurrentLocation))
	{
		return false;
	}
	W.I<int64>(D.Map.Revision);

	// 挑战进度域（schema >= 8 才有）。
	if (D.SchemaVersion >= 8 && !ChallengeProgression(W, D.ChallengeProgression))
	{
		return false;
	}

	return IsValidPayloadSize(O.Num());
}

// 解码规范 payload（与 EncodeCanonicalPayload 对称）。解码过程中会顺带做
// 「规范性校验」：凡是发现排序/字段不在预期位置，返回 NonCanonical 而不是
// InvalidPayload——区别在于 InvalidPayload 表示数据损坏/越界，NonCanonical 表示
// 数据结构合法但字节不是该数据应有的唯一表示。
EHSRSaveDecodeResult HSRSaveVersion::DecodeCanonicalPayload(const TArray<uint8>& P, FHSRSaveData& O)
{
	FReader R(P);
	FHSRSaveData D;
	D.SchemaVersion = R.I<uint32>();
	const uint32 N = R.I<uint32>();
	if (!R.bOk || N > MaxCount)
	{
		return EHSRSaveDecodeResult::InvalidPayload;
	}

	// 角色档案：要求按 CharacterId 严格递增。
	FName PreviousProfile;
	for (uint32 I = 0; I < N; ++I)
	{
		FHSRSaveProfileDto X;
		if (!Profile(R, X) || (!PreviousProfile.IsNone() && !NameLess(PreviousProfile, X.State.CharacterId)))
		{
			return EHSRSaveDecodeResult::NonCanonical;
		}
		PreviousProfile = X.State.CharacterId;
		D.Profiles.Add(MoveTemp(X));
	}

	// 队伍槽位：数量必须等于该 schema 的宽度。
	const uint32 PartyCount = R.I<uint32>();
	if (!R.bOk || PartyCount != PartySlotCountForSchema(D.SchemaVersion))
	{
		return EHSRSaveDecodeResult::NonCanonical;
	}
	for (uint32 I = 0; I < PartyCount; ++I)
	{
		FHSRPartySlot S;
		if (!R.Name(S.CharacterId))
		{
			return EHSRSaveDecodeResult::NonCanonical;
		}
		D.PartySlots.Add(S);
	}
	D.PartyRevision = R.I<int64>();

	if (D.SchemaVersion >= 7)
	{
		// 装备注册表：InstanceId 严格递增。
		if (!ReadCount(R, D.EquipmentRegistry))
		{
			return EHSRSaveDecodeResult::InvalidPayload;
		}
		FGuid Prev;
		for (int32 I = 0; I < D.EquipmentRegistry.Num(); ++I)
		{
			auto& X = D.EquipmentRegistry[I];
			if (!R.NameRequired(X.DefinitionId))
			{
				return EHSRSaveDecodeResult::NonCanonical;
			}
			X.InstanceId = R.Guid();
			X.Kind = R.I<int32>();
			X.EnhancementLevel = R.I<int32>();
			if (!X.InstanceId.IsValid() || (I && !(Prev < X.InstanceId)) || X.Kind < 0 || X.Kind > 1)
			{
				return EHSRSaveDecodeResult::NonCanonical;
			}
			Prev = X.InstanceId;
			const uint32 M = R.I<uint32>();
			if (!R.bOk || M > MaxCount)
			{
				return EHSRSaveDecodeResult::InvalidPayload;
			}
			X.Modifiers.SetNum(M);
			for (auto& V : X.Modifiers)
			{
				const uint8 S = R.U8();
				if (S > static_cast<uint8>(EHSREquipmentStat::Speed))
				{
					return EHSRSaveDecodeResult::NonCanonical;
				}
				V.Stat = static_cast<EHSREquipmentStat>(S);
				double Z;
				if (!R.D(Z))
				{
					return EHSRSaveDecodeResult::NonCanonical;
				}
				V.Value = Z;
			}
			if (!R.Name(X.SetId))
			{
				return EHSRSaveDecodeResult::NonCanonical;
			}
		}
		// 装备摆放：按 CharacterId/Kind/Slot 严格递增，槽位不能重复。
		if (!ReadCount(R, D.EquipmentPlacements))
		{
			return EHSRSaveDecodeResult::InvalidPayload;
		}
		FHSREquipmentPlacementDto PreviousPlacement;
		for (int32 I = 0; I < D.EquipmentPlacements.Num(); ++I)
		{
			auto& X = D.EquipmentPlacements[I];
			X.InstanceId = R.Guid();
			X.CharacterId = R.Guid();
			X.Kind = R.I<int32>();
			X.Slot = R.I<int32>();
			X.AuthorityRevision = R.I<int32>();
			if (!X.InstanceId.IsValid() || !X.CharacterId.IsValid() || X.Kind < 0 || X.Kind > 1 || X.Slot < 0
				|| (X.Kind == 0 && X.Slot > static_cast<int32>(EHSREquipmentSlot::Feet))
				|| (X.Kind == 1 && X.Slot > static_cast<int32>(EHSRRelicSlot::LinkRope))
				|| X.AuthorityRevision < 0)
			{
				return EHSRSaveDecodeResult::NonCanonical;
			}
			if (I && (X.CharacterId < PreviousPlacement.CharacterId
				|| (X.CharacterId == PreviousPlacement.CharacterId
					&& (X.Kind < PreviousPlacement.Kind
						|| (X.Kind == PreviousPlacement.Kind && X.Slot <= PreviousPlacement.Slot)))))
			{
				return EHSRSaveDecodeResult::NonCanonical;
			}
			PreviousPlacement = X;
		}
	}
	else
	{
		// 旧 schema 的扁平装备数组：InstanceId 严格递增。
		if (!ReadCount(R, D.Equipment))
		{
			return EHSRSaveDecodeResult::InvalidPayload;
		}
		FGuid PrevGuid;
		for (int32 I = 0; I < D.Equipment.Num(); ++I)
		{
			auto& X = D.Equipment[I];
			if (!R.NameRequired(X.DefinitionId))
			{
				return EHSRSaveDecodeResult::NonCanonical;
			}
			X.InstanceId = R.Guid();
			X.CharacterId = R.Guid();
			X.Kind = R.I<int32>();
			X.Slot = R.I<int32>();
			X.EnhancementLevel = R.I<int32>();
			if (!X.InstanceId.IsValid() || !X.CharacterId.IsValid() || X.Kind < 0 || X.Kind > 1 || X.Slot < 0
				|| (X.Kind == 0 && X.Slot > static_cast<int32>(EHSREquipmentSlot::Feet))
				|| (X.Kind == 1 && X.Slot > static_cast<int32>(EHSRRelicSlot::LinkRope)))
			{
				return EHSRSaveDecodeResult::NonCanonical;
			}
			const uint32 M = R.I<uint32>();
			if (!R.bOk || M > MaxCount)
			{
				return EHSRSaveDecodeResult::InvalidPayload;
			}
			X.Modifiers.SetNum(M);
			for (auto& V : X.Modifiers)
			{
				const uint8 Stat = R.U8();
				if (Stat > static_cast<uint8>(EHSREquipmentStat::Speed))
				{
					return EHSRSaveDecodeResult::NonCanonical;
				}
				V.Stat = static_cast<EHSREquipmentStat>(Stat);
				double Z;
				if (!R.D(Z))
				{
					return EHSRSaveDecodeResult::NonCanonical;
				}
				V.Value = Z;
			}
			if (!R.Name(X.SetId))
			{
				return EHSRSaveDecodeResult::NonCanonical;
			}
			X.AuthorityRevision = R.I<int32>();
			if (I && !(PrevGuid < X.InstanceId))
			{
				return EHSRSaveDecodeResult::NonCanonical;
			}
			PrevGuid = X.InstanceId;
		}
	}

	// 背包/奖励/任务。
	if (!Inventory(R, D.Inventory) || !Reward(R, D.Rewards) || !Quest(R, D.Quests))
	{
		return EHSRSaveDecodeResult::NonCanonical;
	}

	// 地图域。
	if (!Names(R, D.Map.UnlockedRegionIds) || !Names(R, D.Map.UnlockedTeleportIds)
		|| !Names(R, D.Map.ExplorationFlags) || !Location(R, D.Map.CurrentLocation))
	{
		return EHSRSaveDecodeResult::NonCanonical;
	}
	D.Map.Revision = R.I<int64>();

	// 挑战进度域（schema >= 8）。
	if (D.SchemaVersion >= 8 && !ChallengeProgression(R, D.ChallengeProgression))
	{
		return EHSRSaveDecodeResult::NonCanonical;
	}

	// 收尾校验：读取端状态完好，且游标恰好消费完整个载荷（不允许多余字节）。
	if (!R.bOk || R.P != P.Num())
	{
		return EHSRSaveDecodeResult::InvalidPayload;
	}

	O = MoveTemp(D);
	return EHSRSaveDecodeResult::Success;
}

// MigrateToCurrent：把解码出的旧 schema 数据逐级迁移到 CurrentSchema。
// 迁移是逐 schema 递增的：每步只做「该版本新增了什么域」的补齐/重置，绝不改写
// 其他域，因此迁移结果与迁移前数据在语义上一致，只是补上了新版期望的字段。
EHSRSaveDecodeResult HSRSaveVersion::MigrateToCurrent(FHSRSaveData& D)
{
	if (D.SchemaVersion < 1)
	{
		return EHSRSaveDecodeResult::TooOld;
	}
	if (D.SchemaVersion > CurrentSchema)
	{
		return EHSRSaveDecodeResult::FutureSchema;
	}

	while (D.SchemaVersion < CurrentSchema)
	{
		switch (D.SchemaVersion)
		{
		case 1:
			// schema 1 -> 2：schema 1 的装备数组已废弃，背包/奖励域还没有。
			D.Equipment.Reset();
			D.Inventory = FHSRInventorySaveData();
			D.Rewards = FHSRRewardSaveData();
			++D.SchemaVersion;
			break;
		case 2:
			// schema 2 -> 3：背包/奖励域正式启用（此前只是占位）。
			D.Inventory = FHSRInventorySaveData();
			D.Rewards = FHSRRewardSaveData();
			++D.SchemaVersion;
			break;
		case 3:
			// schema 3 -> 4：新增任务域。
			D.Quests = FHSRQuestSaveData();
			++D.SchemaVersion;
			break;
		case 4:
			// schema 4 -> 5：新增地图域。
			D.Map = FHSRMapSaveData();
			++D.SchemaVersion;
			break;
		case 5:
			// schema 5 -> 6：无新增域（宽度/占位版本）。
			++D.SchemaVersion;
			break;
		case 6:
			// schema 6 -> 7：把扁平装备数组展开为 registry + placement 两份表。
			for (const auto& X : D.Equipment)
			{
				FHSREquipmentRegistryDto R;
				R.DefinitionId = X.DefinitionId;
				R.InstanceId = X.InstanceId;
				R.Kind = X.Kind;
				R.EnhancementLevel = X.EnhancementLevel;
				R.Modifiers = X.Modifiers;
				R.SetId = X.SetId;
				D.EquipmentRegistry.Add(MoveTemp(R));

				FHSREquipmentPlacementDto P;
				P.InstanceId = X.InstanceId;
				P.CharacterId = X.CharacterId;
				P.Kind = X.Kind;
				P.Slot = X.Slot;
				P.AuthorityRevision = X.AuthorityRevision;
				D.EquipmentPlacements.Add(P);
			}
			D.Equipment.Reset();
			++D.SchemaVersion;
			break;
		case 7:
			// schema 7 -> 8：新增挑战进度域。
			D.ChallengeProgression = FHSRChallengeProgressionSaveData();
			++D.SchemaVersion;
			break;
		case 8:
			// schema 8 -> 9：队伍宽度从旧值加宽到 PartySlotCount。
			// 只允许加宽（旧数据槽位更少），不允许收窄——否则会丢角色。
			{
				const int32 Widened = static_cast<int32>(PartySlotCount);
				if (D.PartySlots.Num() > Widened)
				{
					return EHSRSaveDecodeResult::MigrationFailed;
				}
				while (D.PartySlots.Num() < Widened)
				{
					D.PartySlots.Add(FHSRPartySlot());
				}
			}
			++D.SchemaVersion;
			break;
		default:
			return EHSRSaveDecodeResult::MigrationFailed;
		}
	}

	// 迁移完成后的最终一致性校验：宽度精确、各版本号非负、挑战进度域合法、
	// 每个角色档案版本号/等级/经验/突破合法。
	if (D.PartySlots.Num() != static_cast<int32>(PartySlotCount))
	{
		return EHSRSaveDecodeResult::MigrationFailed;
	}
	if (D.PartyRevision < 0 || D.Inventory.Revision < 0 || D.Rewards.Revision < 0
		|| D.Quests.Revision < 0 || D.Map.Revision < 0
		|| !UHSRChallengeProgressionSubsystem::ValidateSaveData(D.ChallengeProgression))
	{
		return EHSRSaveDecodeResult::MigrationFailed;
	}
	for (const FHSRSaveProfileDto& P : D.Profiles)
	{
		if (P.RuntimeRevision < 0 || P.State.Level < 1 || P.State.Experience < 0 || P.State.Ascension < 0)
		{
			return EHSRSaveDecodeResult::MigrationFailed;
		}
	}
	return EHSRSaveDecodeResult::Success;
}

// 计算字节数组的 SHA-256（32 字节）。空数组直接拒绝。
bool HSRSaveVersion::ComputeSha256(const TArray<uint8>& Bytes, TArray<uint8>& Out)
{
	if (!Bytes.IsEmpty() && Bytes.Num() > MAX_uint32)
	{
		return false;
	}
	Out.SetNumUninitialized(32);
	::ComputeSha256(Bytes.GetData(), Bytes.Num(), Out.GetData());
	return true;
}

// EncodeEnvelopeAtUtc：把完整存档打包成「信封」。信封结构固定：
//   magic(8) + 版本/编解码信息(若干) + 载荷长度 + SaveId + 代数 + 时间戳 +
//   槽位身份 + 32 字节校验和占位(写入时填 0 计算后回填) + 载荷。
// 信封头里的 SaveId 在多次保存之间保持不变、Generation 单调递增，供备份恢复
// 时判断血缘关系。
bool HSRSaveVersion::EncodeEnvelopeAtUtc(const FHSRSaveData& D, const FString& Slot, int32 User, const FGuid& Id, uint64 Gen, int64 Utc, TArray<uint8>& O)
{
	if (!IsValidSlot(Slot, User) || !Id.IsValid() || Gen == 0)
	{
		return false;
	}

	// 先编码规范载荷，任何一步失败都不产生信封。
	TArray<uint8> P;
	if (!EncodeCanonicalPayload(D, P))
	{
		return false;
	}

	// 头部字段布局（与 DecodeEnvelope 一一对应，偏移量必须保持同步）。
	O.Reset();
	FWriter W(O);
	const uint8 Magic[8] = { 'H', 'S', 'R', 'S', 'A', 'V', 'E', 0 };
	O.Append(Magic, 8);
	W.I<uint16>(1);                       // FormatVersion
	W.I<uint16>(104);                     // HeaderBytes
	W.I<uint32>(D.SchemaVersion);         // 写入时的 schema
	W.I<uint32>(1);                       // MinimumCompatibleSchema
	W.I<uint16>(1);                       // PayloadCodecVersion
	W.I<uint16>(0);                       // Flags
	W.I<uint64>(P.Num());                 // 载荷长度
	W.Guid(Id);                           // SaveId
	W.I<uint64>(Gen);                     // Generation
	W.I<int64>(Utc);                      // 时间戳（Unix 毫秒）
	W.I<uint64>(MakeSlotIdentity(Slot, User)); // 槽位身份
	for (int32 I = 0; I < 32; ++I)
	{
		W.U8(0);                          // 校验和占位
	}
	O.Append(P);                          // 载荷

	// 计算整个信封的 SHA-256 并回填到偏移 72 处的 32 字节（写时置零，算后回填）。
	TArray<uint8> Digest;
	if (!ComputeSha256(O, Digest))
	{
		return false;
	}
	FMemory::Memcpy(O.GetData() + 72, Digest.GetData(), 32);
	return true;
}

// 以当前 UTC 时间戳调用 EncodeEnvelopeAtUtc。
bool HSRSaveVersion::EncodeEnvelope(const FHSRSaveData& D, const FString& Slot, int32 User, const FGuid& Id, uint64 Gen, TArray<uint8>& O)
{
	return EncodeEnvelopeAtUtc(D, Slot, User, Id, Gen, FDateTime::UtcNow().ToUnixTimestamp() * 1000, O);
}

// DecodeEnvelope：解析并验证信封。校验链：
//   槽位/长度/魔数 -> 头字段(格式/编解码/标志) -> SaveId/代数 -> 载荷大小 -> 槽位身份
//   -> SHA-256 校验和 -> schema 边界 -> 最小兼容版本 -> 规范载荷解码 -> schema 一致性
//   -> 迁移到当前 schema。
EHSRSaveDecodeResult HSRSaveVersion::DecodeEnvelope(const TArray<uint8>& B, const FString& Slot, int32 User, FHSRSaveData& O, FHSRSaveEnvelopeHeader* H)
{
	if (!IsValidSlot(Slot, User))
	{
		return EHSRSaveDecodeResult::InvalidArgument;
	}
	if (B.Num() < FHSRSaveEnvelopeHeader::HeaderBytes)
	{
		return EHSRSaveDecodeResult::TooShort;
	}

	// 魔数：不是本格式直接判 BadMagic（调用方会尝试旧的 USaveGame 兼容路径）。
	const uint8 Magic[8] = { 'H', 'S', 'R', 'S', 'A', 'V', 'E', 0 };
	if (FMemory::Memcmp(B.GetData(), Magic, 8) != 0)
	{
		return EHSRSaveDecodeResult::BadMagic;
	}

	// 读取头字段。
	FReader R(B);
	R.P = 8;
	const uint16 Format = R.I<uint16>(), HeaderSize = R.I<uint16>();
	FHSRSaveEnvelopeHeader X;
	X.SchemaVersion = R.I<uint32>();
	X.MinimumCompatibleSchema = R.I<uint32>();
	const uint16 Codec = R.I<uint16>(), Flags = R.I<uint16>();
	const uint64 Size = R.I<uint64>();
	X.SaveId = R.Guid();
	X.Generation = R.I<uint64>();
	X.UtcUnixMilliseconds = R.I<int64>();
	X.SlotIdentity = R.I<uint64>();
	if (!R.bOk)
	{
		return EHSRSaveDecodeResult::InvalidHeader;
	}

	// 格式/编解码/标志必须完全匹配，否则视为不支持的格式。
	if (Format != FHSRSaveEnvelopeHeader::FormatVersion || HeaderSize != FHSRSaveEnvelopeHeader::HeaderBytes
		|| Codec != FHSRSaveEnvelopeHeader::PayloadCodecVersion || Flags != 0)
	{
		return EHSRSaveDecodeResult::UnsupportedFormat;
	}
	if (!X.SaveId.IsValid() || X.Generation == 0)
	{
		return EHSRSaveDecodeResult::InvalidHeader;
	}
	if (!IsValidPayloadSize(Size) || Size != static_cast<uint64>(B.Num() - FHSRSaveEnvelopeHeader::HeaderBytes))
	{
		return EHSRSaveDecodeResult::InvalidSize;
	}
	if (X.SlotIdentity != MakeSlotIdentity(Slot, User))
	{
		return EHSRSaveDecodeResult::SlotMismatch;
	}

	// 校验和：把头部偏移 72 处的 32 字节临时置零后重算，与存盘值比对。
	TArray<uint8> Copy = B, Digest;
	FMemory::Memset(Copy.GetData() + 72, 0, 32);
	if (!ComputeSha256(Copy, Digest) || FMemory::Memcmp(B.GetData() + 72, Digest.GetData(), 32) != 0)
	{
		return EHSRSaveDecodeResult::ChecksumMismatch;
	}

	if (H)
	{
		*H = X;
	}

	// schema 边界检查。
	if (X.SchemaVersion > CurrentSchema)
	{
		return EHSRSaveDecodeResult::FutureSchema;
	}
	if (X.SchemaVersion < 1)
	{
		return EHSRSaveDecodeResult::TooOld;
	}
	if (X.MinimumCompatibleSchema < 1 || X.MinimumCompatibleSchema > X.SchemaVersion)
	{
		return EHSRSaveDecodeResult::InvalidHeader;
	}

	// 解出载荷 -> 校验 schema 一致 -> 迁移到当前 schema。
	TArray<uint8> P;
	P.Append(B.GetData() + FHSRSaveEnvelopeHeader::HeaderBytes, Size);
	EHSRSaveDecodeResult Result = DecodeCanonicalPayload(P, O);
	if (Result != EHSRSaveDecodeResult::Success)
	{
		return Result;
	}
	if (O.SchemaVersion != X.SchemaVersion)
	{
		return EHSRSaveDecodeResult::NonCanonical;
	}
	return MigrateToCurrent(O);
}
