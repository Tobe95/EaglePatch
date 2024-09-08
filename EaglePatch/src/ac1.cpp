#include <Windows.h>
#include <stdio.h>
#include <stdint.h>
#include <Xinput.h>
#include <dxgi.h>

#include "patcher.h"

#include "../shared/ini_reader.h"

//#define INCLUDE_CONSOLE // add ability to allocate console window

#ifdef INCLUDE_CONSOLE
#include "../shared/console.h"
#endif

enum eExeVersion
{
	DIGITAL_DX9 = 0,
	DIGITAL_DX10
};

// these were made static because otherwise it crashes :shrug:
struct sAddresses
{
	static uintptr_t Pad_UpdateTimeStamps;
	static uintptr_t Pad_ScaleStickValues;
	static uintptr_t PadXenon_ctor;
	static uintptr_t PadProxyPC_AddPad;
	static uintptr_t _addXenonJoy_Patch;
	static uintptr_t _addXenonJoy_JumpOut;
	static uintptr_t _PadProxyPC_Patch;
	static uintptr_t _multisampling1;
	static uintptr_t _multisampling2;
	static uintptr_t _multisampling3;
	static uint32_t* _descriptor_var;
	static uintptr_t _ps3_controls[4];
	static uintptr_t _ps3_controls_analog[4];
	static uintptr_t _skipIntroVideos;
	static uintptr_t _shadowMapSize1;
	static uintptr_t _shadowMapSize2;
	static uintptr_t _shadowMapSize3;
	static uintptr_t _shadowMapSize4;
	static uintptr_t _maxNumNPCs_high1;
	static uintptr_t _maxNumNPCs_high2;
	static uintptr_t _maxNumNPCs_low1;
	static uintptr_t _maxNumNPCs_low2;
	static uintptr_t _increaseFOV;
	static uintptr_t _increaseFOV_JumpOut;
	static uintptr_t _smallObjectsCullDistanceModifier;
	static uintptr_t _smallObjectsCullDistanceModifier2;
	static uintptr_t _smallObjectsCullDistanceModifier_JumpOut;
	static uintptr_t _mediumObjectsCullDistanceModifier;
	static uintptr_t _mediumObjectsCullDistanceModifier2;
	static uintptr_t _mediumObjectsCullDistanceModifier_JumpOut;
};

uintptr_t sAddresses::Pad_UpdateTimeStamps = 0;
uintptr_t sAddresses::Pad_ScaleStickValues = 0;
uintptr_t sAddresses::PadXenon_ctor = 0;
uintptr_t sAddresses::PadProxyPC_AddPad = 0;
uintptr_t sAddresses::_addXenonJoy_Patch = 0;
uintptr_t sAddresses::_addXenonJoy_JumpOut = 0;
uintptr_t sAddresses::_PadProxyPC_Patch = 0;
uintptr_t sAddresses::_multisampling1 = 0;
uintptr_t sAddresses::_multisampling2 = 0;
uintptr_t sAddresses::_multisampling3 = 0;
uint32_t* sAddresses::_descriptor_var = 0;
uintptr_t sAddresses::_ps3_controls[4];
uintptr_t sAddresses::_ps3_controls_analog[4];
uintptr_t sAddresses::_skipIntroVideos = 0;
uintptr_t sAddresses::_shadowMapSize1 = 0;
uintptr_t sAddresses::_shadowMapSize2 = 0;
uintptr_t sAddresses::_shadowMapSize3 = 0;
uintptr_t sAddresses::_shadowMapSize4 = 0;
uintptr_t sAddresses::_maxNumNPCs_high1 = 0;
uintptr_t sAddresses::_maxNumNPCs_high2 = 0;
uintptr_t sAddresses::_maxNumNPCs_low1 = 0;
uintptr_t sAddresses::_maxNumNPCs_low2 = 0;
uintptr_t sAddresses::_increaseFOV = 0;
uintptr_t sAddresses::_increaseFOV_JumpOut = 0;
uintptr_t sAddresses::_smallObjectsCullDistanceModifier = 0;
uintptr_t sAddresses::_smallObjectsCullDistanceModifier2 = 0;
uintptr_t sAddresses::_smallObjectsCullDistanceModifier_JumpOut = 0;
uintptr_t sAddresses::_mediumObjectsCullDistanceModifier = 0;
uintptr_t sAddresses::_mediumObjectsCullDistanceModifier2 = 0;
uintptr_t sAddresses::_mediumObjectsCullDistanceModifier_JumpOut = 0;

auto ac_getNewDescriptor = (void* (__cdecl*)(uint32_t, uint32_t, uint32_t))0;
auto ac_allocate = (void*(__cdecl*)(int, uint32_t, void*, const void*, const char*, const char*, uint32_t, const char*))0;
auto ac_delete = (void (__cdecl*)(void*, void*, const char*))0;

static int NEEDED_KEYBOARD_SET = 0;
static int DESIRED_SHADOWMAP_SIZE = 0;

namespace scimitar
{
	struct Object
	{
		void** vtable;
	};

	struct ManagedObject : Object
	{
		int m_Flags;
	};

	class InputBindings;

	struct Pad : ManagedObject
	{
		enum PadType
		{
			MouseKeyboardPad = 0,
			PCPad,
			XenonPad,
			PS2Pad,
		};

		enum PadButton
		{
			Button1,
			Button2,
			Button3,
			Button4,
			PadDown,
			PadLeft,
			PadUp,
			PadRight,
			Select,
			Start,
			ShoulderLeft1,
			ShoulderLeft2,
			ShoulderRight1,
			ShoulderRight2,
			StickLeft,
			StickRight,
			NbButtons,
			Button_Invalid = -1,
		};

		struct ButtonStates
		{
			bool state[NbButtons];

			bool IsEmpty() const
			{
				for (int i = 0; i < NbButtons; i++)
					if (state[i])
						return false;
				return true;
			}
		};

		struct AnalogButtonStates
		{
			float state[NbButtons];
		};

		struct __declspec(align(16)) StickState
		{
			float x, y;

			bool IsEmpty() const
			{
				//return fabsf(x) < 0.1f && fabsf(y) < 0.1f;
				return x == 0.0f && y == 0.0f;
			}
		};

		ButtonStates m_LastFrame;
		ButtonStates m_ThisFrame;
		uint64_t m_LastFrameTimeStamp;
		uint64_t m_ThisFrameTimeStamp;
		uint64_t m_ButtonPressTimeStamp[NbButtons];
		AnalogButtonStates m_ButtonValues;
		StickState LeftStick;
		StickState RightStick;
		int field_1B0[17];
		float* vibrationData;
		char field_1F8[0x390];

		void UpdatePad(InputBindings*a)
		{
			((void(__thiscall*)(Pad*, InputBindings*))vtable[10])(this, a);
		}

		void UpdateTimeStamps()
		{
			((void(__thiscall*)(Pad*))sAddresses::Pad_UpdateTimeStamps)(this);
		}

		void ScaleStickValues()
		{
			((void(__thiscall*)(Pad*))sAddresses::Pad_ScaleStickValues)(this);
		}

		bool IsEmpty() const
		{
			return m_ThisFrame.IsEmpty() && LeftStick.IsEmpty() && RightStick.IsEmpty();
		}
	};
	static_assert(sizeof(Pad) == 0x500, "Pad");

	struct PadXenon : Pad
	{
		struct PadState
		{
			XINPUT_CAPABILITIES Caps;
			bool Connected;
			bool Inserted;
			bool Removed;
		};

		uint32_t m_PadIndex;
		PadState m_PadState;

		PadXenon* _ctor(uint32_t padId)
		{
			return ((PadXenon *(__thiscall*)(PadXenon*, uint32_t))sAddresses::PadXenon_ctor)(this, padId);
		}

		PadXenon(uint32_t padId)
		{
			_ctor(padId);
		}
		void* operator new(size_t size)
		{
			return ac_allocate(2, sizeof(PadXenon), ac_getNewDescriptor(sizeof(PadXenon), 16, *sAddresses::_descriptor_var), nullptr, nullptr, nullptr, 0, nullptr);
		}
	};

	static_assert(sizeof(PadXenon) == 0x520, "PadXenon");

	struct PadData
	{
		scimitar::Pad* pad;
		char field_4[528];
		InputBindings* pInputBindings;
	};
	static_assert(sizeof(PadData) == 536, "PadData");

	enum PadSets
	{
		Keyboard1 = 0,
		Keyboard2,
		Keyboard3,
		Keyboard4,
		Joy1,
		Joy2,
		Joy3,
		Joy4,
		NbPadSets,
	};

	struct PadProxyPC : Pad
	{
		int field_590;
		uint32_t selectedPad;
		int field_598;
		scimitar::PadData pads[NbPadSets];

		bool AddPad(scimitar::Pad* a, PadType b, const wchar_t* c, uint16_t d, uint16_t e)
		{
			return ((bool(__thiscall*)(PadProxyPC*, scimitar::Pad*, PadType, const wchar_t*, uint16_t, uint16_t))sAddresses::PadProxyPC_AddPad)(this,a,b,c,d,e);
		}

		void Update()
		{
			if (selectedPad != NEEDED_KEYBOARD_SET && selectedPad != Joy1)
				selectedPad = NEEDED_KEYBOARD_SET;

			pads[NEEDED_KEYBOARD_SET].pad->UpdatePad(pads[NEEDED_KEYBOARD_SET].pInputBindings);
			pads[Joy1].pad->UpdatePad(pads[Joy1].pInputBindings);

			// see if no button was pressed in current pad
			if (pads[selectedPad].pad->IsEmpty())
			{
				uint32_t i = selectedPad == NEEDED_KEYBOARD_SET ? Joy1 : NEEDED_KEYBOARD_SET;
				// if any button was pressed on the other pad then switch to it
				if (pads[i].pad && !pads[i].pad->IsEmpty())
					selectedPad = i;
			}

			m_LastFrame = m_ThisFrame;
			m_ThisFrame = pads[selectedPad].pad->m_ThisFrame;
			LeftStick = pads[selectedPad].pad->LeftStick;
			RightStick = pads[selectedPad].pad->RightStick;

			if (selectedPad >= Joy1) // FIX: Use genuine analog values for gamepads
				m_ButtonValues = pads[selectedPad].pad->m_ButtonValues;
			else
			{
				// This is what original code does with any pad
				// I didn't bother to check if keyboard code fills m_ButtonValues so I'll leave this in just in case
				for (uint32_t i = 0; i < NbButtons; i++)
					m_ButtonValues.state[i] = m_ThisFrame.state[i] ? 1.0f : 0.0f;
			}

			UpdateTimeStamps();
		}
	};
	static_assert(sizeof(PadProxyPC) == 0x15D0, "PadProxyPC");
}

static scimitar::PadProxyPC* pPad = nullptr;
static scimitar::PadXenon* padXenon = nullptr;

void __cdecl AddXenonPad()
{
	padXenon = new scimitar::PadXenon(0);
	if (!pPad->AddPad(padXenon, scimitar::Pad::PadType::XenonPad, L"XInput Controller 1", 5, 5))
		ac_delete(padXenon, nullptr, nullptr);
}

ASM(_addXenonJoy_Patch)
{
	__asm
	{
		mov eax, [eax+4]
		mov pPad, eax
		call AddXenonPad
	}
	VARJMP(sAddresses::_addXenonJoy_JumpOut)
}

ASM(_FOV_Patch)
{
	__asm
	{
		mov dword ptr [esi + 0xA0], 0x3F800000 //,(float)1 // FOV default=0.8
		movss xmm1,dword ptr [esi + 0xA0]
		jmp sAddresses::_increaseFOV_JumpOut
	}
}

ASM(_smallObjectsCullDistanceModifier_Patch)
{
	__asm
	{
		mov dword ptr [edi + 0x44], 0x00
		jmp sAddresses::_smallObjectsCullDistanceModifier_JumpOut
	}
}

ASM(_mediumObjectsCullDistanceModifier_Patch)
{
	__asm
	{
		mov dword ptr [edi + 0x48], 0x00
		jmp sAddresses::_mediumObjectsCullDistanceModifier_JumpOut
	}
}

struct D3D10ResolutionContainer
{
	IDXGIOutput* DXGIOutput;
	DXGI_MODE_DESC* modes;
	uint32_t m_width, m_height, m_refreshRate, _5, _6;
	uint32_t modesNum;
	uint32_t _8;
	// there's probably more fields in here but we don't need them

	void GetDisplayModes(IDXGIOutput* a1)
	{
		((void(__thiscall*)(D3D10ResolutionContainer*, IDXGIOutput*))0x7BAD20)(this, a1);
	}

	void FindCurrentResolutionMode(uint32_t width, uint32_t height, uint32_t refreshRate)
	{
		((void(__thiscall*)(D3D10ResolutionContainer*, uint32_t, uint32_t, uint32_t))0x7BA770)(this, width, height, refreshRate);
	}

	static bool IsDisplayModeAlreadyAdded(const DXGI_MODE_DESC& mode, const DXGI_MODE_DESC* newModes, const uint32_t newModesNum)
	{
		for (uint32_t j = newModesNum; j > 0; j--)
		{
			if (newModes[j - 1].Width == mode.Width && newModes[j - 1].Height == mode.Height
				&& newModes[j - 1].RefreshRate.Numerator == mode.RefreshRate.Numerator
				&& newModes[j - 1].Format == mode.Format && newModes[j - 1].ScanlineOrdering == mode.ScanlineOrdering)
				return true;
		}
		return false;
	}

	void GetDisplayModes_hook(IDXGIOutput* a1)
	{
		GetDisplayModes(a1);
		DXGI_MODE_DESC* newModes = new DXGI_MODE_DESC[modesNum];
		uint32_t newModesNum = 0;
		for (uint32_t i = 0; i < modesNum; i++)
		{
			if (!IsDisplayModeAlreadyAdded(modes[i], newModes, newModesNum))
			{
				newModes[newModesNum++] = modes[i];
//#ifdef INCLUDE_CONSOLE
//				printf("Mode %i - Width %i Height %i RefreshRate %i %i Format %i ScanlineOrdering %i Scaling %i\n", i,
//					modes[i].Width, modes[i].Height, modes[i].RefreshRate.Numerator, modes[i].RefreshRate.Denominator, modes[i].Format, modes[i].ScanlineOrdering, modes[i].Scaling);
//#endif
			}
		}
		memset(modes, 0, sizeof(DXGI_MODE_DESC) * modesNum); // just to have cleaner memory
		memcpy(modes, newModes, sizeof(DXGI_MODE_DESC) * newModesNum);
		modesNum = newModesNum;
		delete[]newModes;
		FindCurrentResolutionMode(m_width, m_height, m_refreshRate);
	}
};

void patch()
{
	NEEDED_KEYBOARD_SET = get_private_profile_int("KeyboardLayout", scimitar::PadSets::Keyboard1);
	if (NEEDED_KEYBOARD_SET < scimitar::PadSets::Keyboard1) NEEDED_KEYBOARD_SET = scimitar::PadSets::Keyboard1;
	else if (NEEDED_KEYBOARD_SET > scimitar::PadSets::Keyboard4) NEEDED_KEYBOARD_SET = scimitar::PadSets::Keyboard4;

	InjectHook(sAddresses::_addXenonJoy_Patch, &_addXenonJoy_Patch, PATCH_JUMP);
	InjectHook(sAddresses::_PadProxyPC_Patch, &scimitar::PadProxyPC::Update, PATCH_JUMP);

	// fix multisampling
	PatchByte(sAddresses::_multisampling1, 0xEB);
	PatchBytes(sAddresses::_multisampling2, (unsigned char*)"\xb9\x01\x00\x00\x00\x90", 6);
	Nop(sAddresses::_multisampling3, 3);

	if (get_private_profile_bool("PS3Controls", FALSE))
	{
		PatchByte(sAddresses::_ps3_controls[0], 0x23);
		PatchByte(sAddresses::_ps3_controls[1], 0x25);
		PatchByte(sAddresses::_ps3_controls[2], 0x22);
		PatchByte(sAddresses::_ps3_controls[3], 0x24);
		Patch<uint32_t>(sAddresses::_ps3_controls_analog[0], 0xE4);
		Patch<uint32_t>(sAddresses::_ps3_controls_analog[1], 0xEC);
		Patch<uint32_t>(sAddresses::_ps3_controls_analog[2], 0xE0);
		Patch<uint32_t>(sAddresses::_ps3_controls_analog[3], 0xE8);
	}

	if (get_private_profile_bool("SkipIntroVideos", FALSE))
		PatchByte(sAddresses::_skipIntroVideos, 0xEB);

	if (get_private_profile_bool("OverwriteCrowdDensity", FALSE))
	{
		// 0x400 is just random, it doesn't scale that high
		PatchBytes(sAddresses::_maxNumNPCs_high1, (unsigned char*)"\x00\x04", 2);
		PatchBytes(sAddresses::_maxNumNPCs_high2, (unsigned char*)"\x00\x04", 2);
		PatchByte(sAddresses::_maxNumNPCs_low1, 0x00);
		PatchByte(sAddresses::_maxNumNPCs_low2, 0x00);
	}

	if (get_private_profile_bool("IncreaseFOV", FALSE))
	InjectHook(sAddresses::_increaseFOV, _FOV_Patch, HOOK_JUMP);

	if (get_private_profile_bool("ImproveDrawDistance", TRUE))
	{
		// This slightly improves draw distance of smaller objects such as boxes and stuff on highest detail setting. 
		// The same can be achieved by changing the respective culldistancemodifier in Assassin.ini from "1" to "0" and setting file permissions to read-only.
		BYTE smallObjectsCullDistance[] = {
			0xC7, 0x44, 0x20, 0x44, 0x00, 0x00, 0x00, 0x00, // mov dword ptr [EAX + 0x44],0x0
			0x90, 0x90, 0x90, 0x90, 0x90 // nop; nop; nop; nop; nop
		};
		PatchBytes(sAddresses::_smallObjectsCullDistanceModifier, smallObjectsCullDistance);
		
		BYTE mediumObjectsCullDistance[] = {
	        0xC7, 0x44, 0x20, 0x48, 0x00, 0x00, 0x00, 0x00, // mov dword ptr [EAX + 0x48],0x0
			0x8B, 0xCD, // mov exc, ebp
			0x90, 0x90, 0x90, 0x90, 0x90 // nop; nop; nop; nop; nop
		};
		PatchBytes(sAddresses::_mediumObjectsCullDistanceModifier, mediumObjectsCullDistance);

		InjectHook(sAddresses::_smallObjectsCullDistanceModifier2, _smallObjectsCullDistanceModifier_Patch, HOOK_JUMP);
		InjectHook(sAddresses::_mediumObjectsCullDistanceModifier2, _mediumObjectsCullDistanceModifier_Patch, HOOK_JUMP);

	}
}

void InitAddresses(eExeVersion exeVersion)
{
	init_private_profile();
#ifdef INCLUDE_CONSOLE
	if (get_private_profile_bool("AllocConsole", FALSE)) init_console();
#endif

	switch (exeVersion)
	{
	case DIGITAL_DX9:
		sAddresses::Pad_UpdateTimeStamps = 0x93F990;
		sAddresses::Pad_ScaleStickValues = 0x93FC80;
		sAddresses::PadXenon_ctor = 0x9161A0;
		sAddresses::PadProxyPC_AddPad = 0x90B2F0;
		sAddresses::_addXenonJoy_Patch = 0x916979;
		sAddresses::_addXenonJoy_JumpOut = 0x916990;
		sAddresses::_PadProxyPC_Patch = 0x909C90;
		sAddresses::_multisampling1 = 0xE91422;
		sAddresses::_multisampling2 = 0xE9116D;
		sAddresses::_multisampling3 = 0xE91178;
		sAddresses::_descriptor_var = (uint32_t*)0x1A1E680;
		ac_getNewDescriptor = (void*(__cdecl*)(uint32_t, uint32_t, uint32_t))0x924070;
		ac_allocate = (void* (__cdecl*)(int, uint32_t, void*, const void*, const char*, const char*, uint32_t, const char*))0x7A4510;
		ac_delete = (void(__cdecl*)(void*, void*, const char*))0x916440;

		sAddresses::_ps3_controls[0] = 0x98CF98 + 2;
		sAddresses::_ps3_controls[1] = 0x98CFB5 + 2;
		sAddresses::_ps3_controls[2] = 0x98D061 + 2;
		sAddresses::_ps3_controls[3] = 0x98D064 + 2;
		sAddresses::_ps3_controls_analog[0] = 0x98D02E + 4;
		sAddresses::_ps3_controls_analog[1] = 0x98D056 + 4;
		sAddresses::_ps3_controls_analog[2] = 0x98D07B + 4;
		sAddresses::_ps3_controls_analog[3] = 0x98D092 + 4;
		sAddresses::_skipIntroVideos = 0x405495;
		sAddresses::_shadowMapSize1 = 0x9D97C3;
		sAddresses::_shadowMapSize2 = 0xE7F281;
		sAddresses::_shadowMapSize3 = 0xE911FA;
		sAddresses::_shadowMapSize4 = 0xE7EC4C;
		sAddresses::_maxNumNPCs_high1 = 0x546D1F;
		sAddresses::_maxNumNPCs_high2 = 0xE7F4E0;
		sAddresses::_maxNumNPCs_low1 = 0xE7F491;
		sAddresses::_maxNumNPCs_low2 = 0xE7E3FF;
		sAddresses::_increaseFOV = 0x4BFC45;
		sAddresses::_increaseFOV_JumpOut = 0x4BFC4D;
		sAddresses::_smallObjectsCullDistanceModifier = 0xE7F415;
		sAddresses::_smallObjectsCullDistanceModifier2 = 0x9D990B;
		sAddresses::_smallObjectsCullDistanceModifier_JumpOut = 0x9D9910;
		sAddresses::_mediumObjectsCullDistanceModifier = 0xE7F427;
		sAddresses::_mediumObjectsCullDistanceModifier2 = 0x9D9934;
		sAddresses::_mediumObjectsCullDistanceModifier_JumpOut = 0x9D9939;

		// While doing the same for DX10 at 0x7BB443 technically improves shadowmaps, it completely breakes cascade/shadow distances
		DESIRED_SHADOWMAP_SIZE = get_private_profile_int("D3D9_ImproveShadowMapResolution", 1);
		if (DESIRED_SHADOWMAP_SIZE > 0)
		{
			if (DESIRED_SHADOWMAP_SIZE < 2)
			{
				PatchByte(sAddresses::_shadowMapSize1, 0x08);
				PatchByte(sAddresses::_shadowMapSize2, 0x08);
				PatchByte(sAddresses::_shadowMapSize3, 0x08);
				PatchByte(sAddresses::_shadowMapSize4, 0x08);
			}
			else
			{
				PatchByte(sAddresses::_shadowMapSize1, 0x10);
				PatchByte(sAddresses::_shadowMapSize2, 0x10);
				PatchByte(sAddresses::_shadowMapSize3, 0x10);
				PatchByte(sAddresses::_shadowMapSize4, 0x10);
			}
		}

		// This is the Wide Screen Fix from WSGF, there's probably a more elegant way to implement this
		if (get_private_profile_bool("WideScreenFix", FALSE))
		{
			PatchBytes(0x403E5E, (unsigned char*)"\xdb\x44\xe4\x1c\xdc\x0d\xf0\xda\x6b\x01\xdb\x1d\xf8\x6f\xa4\x01\x8b\x0d\xf8\x6f\xa4\x01\xeb\x59", 24);
			PatchBytes(0x403ECD, (unsigned char*)"\xeb\x8f", 2);
			PatchBytes(0x7862FA, (unsigned char*)"\xe9\xab\x31\xef\x00\x90\x90\x90", 8);
			PatchBytes(0x9DC86C, (unsigned char*)"\x00\x00\x7f", 3);
			PatchBytes(0x9DFA78, (unsigned char*)"\xdd\xd9\xd9\xc0\xd8\x7c\xe4\x04\x90\x90\x90\x90", 12);
			PatchBytes(0x9E0A0C, (unsigned char*)"\xe9\x8d\x8c\xc9\x00\x90\x90", 7);
			PatchBytes(0x9E8F59, (unsigned char*)"\xe9\x84\x07\xc9\x00\x90", 6);
			PatchBytes(0x16794AA, (unsigned char*)"\x66\x81\x7b\x0c\x00\x05\x74\x0f\x0f\xb7\x4b\x0c\x0f\xb7\x53\x0e\xe9\x43\xce\x10\xff", 21);
			PatchBytes(0x16794C1, (unsigned char*)"\x81\xb8\x98\x00\x00\x00\x39\x8e\xe3\x3f\x0f\x8f\xc9\x00\x00\x00\xdf\x43\x0c\xd8\xb0\x98\x00\x00\x00\xdb\x54\xe4\x20\x8b\x54\xe4\x20\x0f\xb7\x4b\x0c\xdf\x43\x0e\xd8\xe9\xd8\x0d\x4a\x58\xfe\x00\xd9\x1d\xf0\x6f\xa4\x01\xdd\xd8\x66\x0f\xd6\x44\x24\x20\xf3\x0f\x7e\x80\x8c\x00\x00\x00\x8b\x44\x24\x14\x3b\xe8\x66\x0f\xd6\x44\x24\x28\xf3\x0f\x10\x44\x24\x28\xf2\x0f\x2a\xc9\x0f\x5a\xc0\xf2\x0f\x5e\xc1\xf3\x0f\x10\x4c\x24\x2c\x0f\x5a\xc9\xf2\x0f\x2a\xd2\xf2\x0f\x5e\xca\x66\x0f\x5a\xc0\x66\x0f\x5a\xc9\x0f\x84\x4d\xce\x10\xff\xeb\x06\x8d\x9b\x00\x00\x00\x00\xf3\x0f\x10\x5d\x00\x0f\x5a\xdb\xf3\x0f\x5a\xd0\xf2\x0f\x59\xd3\xf2\x0f\x5a\xd2\xf3\x0f\x11\x55\x00\xf3\x0f\x10\x55\x04\xf3\x0f\x58\x15\xf0\x6f\xa4\x01\x0f\x5a\xd2\xf3\x0f\x5a\xd9\xf2\x0f\x59\xd3\x66\x0f\x5a\xd2\xf3\x0f\x11\x55\x04\x81\x4f\x6c\x00\x02\x00\x00\x83\xc5\x14\x3b\xe8\x75\xb8\xe9\xf8\xcd\x10\xff", 215);
			PatchBytes(0x167959A, (unsigned char*)"\xdf\x43\x0e\xd8\x88\x98\x00\x00\x00\xdb\x54\xe4\x20\x8b\x4c\xe4\x20\x0f\xb7\x53\x0e\xdf\x43\x0c\xd8\xe9\xd8\x0d\x4a\x58\xfe\x00\xd9\x1d\xf0\x6f\xa4\x01\xdd\xd8\x66\x0f\xd6\x44\x24\x20\xf3\x0f\x7e\x80\x8c\x00\x00\x00\x8b\x44\x24\x14\x3b\xe8\x66\x0f\xd6\x44\x24\x28\xf3\x0f\x10\x44\x24\x28\xf2\x0f\x2a\xc9\x0f\x5a\xc0\xf2\x0f\x5e\xc1\xf3\x0f\x10\x4c\x24\x2c\x0f\x5a\xc9\xf2\x0f\x2a\xd2\xf2\x0f\x5e\xca\x66\x0f\x5a\xc0\x66\x0f\x5a\xc9\x0f\x84\x84\xcd\x10\xff\xeb\x06\x8d\x9b\x00\x00\x00\x00\xeb\x4b\x90\x90\x90\xf3\x0f\x58\x1d\xf0\x6f\xa4\x01\x0f\x5a\xdb\xf3\x0f\x5a\xd0\xf2\x0f\x59\xd3\xf2\x0f\x5a\xd2\xf3\x0f\x11\x55\x00\xf3\x0f\x10\x55\x04\x0f\x5a\xd2\xf3\x0f\x5a\xd9\xf2\x0f\x59\xd3\x66\x0f\x5a\xd2\xf3\x0f\x11\x55\x04\x81\x4f\x6c\x00\x02\x00\x00\x83\xc5\x14\x3b\xe8\x75\xb8\xe9\x2f\xcd\x10\xff\x81\x7d\x00\x00\x00\xba\xc3\x74\x2b\x81\x7d\x00\x00\x00\xa8\xc3\x74\x22\x81\x7d\x00\x00\x00\xc2\xc2\x7f\x07\xf3\x0f\x10\x5d\x00\xeb\x96\x81\x7d\x00\x00\x00\x0e\xc3\x7c\x02\xeb\xee\xe9\xc7\x01\x00\x00", 249);
			PatchBytes(0x1679695, (unsigned char*)"\xf3\x0f\x10\x5d\x00\xeb\x85", 7);
			PatchBytes(0x167969E, (unsigned char*)"\x81\x7c\xe4\x0c\x39\x8e\xe3\x3f\x7c\x0e\xd9\x43\x40\xd9\x5c\x24\x08\xe9\x5f\x73\x36\xff", 22);
			PatchBytes(0x16796B6, (unsigned char*)"\xd9\x43\x40\xd8\x0d\x4a\x58\xfe\x00\xd9\xf2\xdd\xd8\xdc\x0d\xf0\xda\x6b\x01\xd8\x74\xe4\x0c\xd9\xe8\xd9\xf3\xd8\x35\x4a\x58\xfe\x00\xd9\x5c\xe4\x08\xe9\x33\x73\x36\xff", 42);
			PatchBytes(0x16796E2, (unsigned char*)"\x81\xb9\x98\x00\x00\x00\x39\x8e\xe3\x3f\x0f\x8c\xcb\x00\x00\x00\xd9\x81\x90\x00\x00\x00\xd9\x7c\x24\x0a\x0f\xb7\x44\x24\x0a\x0d\x00\x0c\x00\x00\x89\x44\x24\x0c\xd9\x6c\x24\x0c\xdf\x7c\x24\x0c\x8b\x54\x24\x0c\x52\xd9\x6c\x24\x0e\xd9\x81\x8c\x00\x00\x00\xd9\x7c\x24\x0e\x0f\xb7\x44\x24\x0e\x0d\x00\x0c\x00\x00\x89\x44\x24\x10\xd9\x6c\x24\x10\xdf\x7c\x24\x10\x8b\x44\x24\x10\x50\xdb\x44\xe4\x04\xdc\x35\xf8\xda\x6b\x01\xdb\x14\xe4\xd8\xa9\x8c\x00\x00\x00\xd8\x0d\x4a\x58\xfe\x00\xdb\x1d\xf4\x6f\xa4\x01\xd9\x6c\x24\x12\xd9\x81\x88\x00\x00\x00\xd9\x7c\x24\x12\x0f\xb7\x44\x24\x12\x0d\x00\x0c\x00\x00\x89\x44\x24\x14\xd9\x6c\x24\x14\xdf\x7c\x24\x14\x8b\x54\x24\x14\x52\xd9\x6c\x24\x16\xd9\x81\x84\x00\x00\x00\xd9\x7c\x24\x16\x8b\xcf\x0f\xb7\x44\x24\x16\x0d\x00\x0c\x00\x00\x89\x44\x24\x18\xd9\x6c\x24\x18\xdf\x7c\x24\x18\x8b\x44\x24\x18\xff\x35\xf4\x6f\xa4\x01\xe9\x3e\xf8\x36\xff\xd9\x81\x8c\x00\x00\x00\xdc\x0d\xf8\xda\x6b\x01\xd9\xc0\xd8\xa9\x90\x00\x00\x00\xd8\x0d\x4a\x58\xfe\x00\xdb\x1d\xf4\x6f\xa4\x01\xd9\x7c\x24\x0a\x0f\xb7\x44\x24\x0a\x0d\x00\x0c\x00\x00\x89\x44\x24\x0c\xd9\x6c\x24\x0c\xdf\x7c\x24\x0c\x8b\x54\x24\x0c\x52\xd9\x6c\x24\x0e\xd9\x81\x8c\x00\x00\x00\xd9\x7c\x24\x0e\x0f\xb7\x44\x24\x0e\x0d\x00\x0c\x00\x00\x89\x44\x24\x10\xd9\x6c\x24\x10\xdf\x7c\x24\x10\x8b\x44\x24\x10\x50\xd9\x6c\x24\x12\xd9\x81\x88\x00\x00\x00\xd9\x7c\x24\x12\x0f\xb7\x44\x24\x12\x0d\x00\x0c\x00\x00\x89\x44\x24\x14\xd9\x6c\x24\x14\xdf\x7c\x24\x14\x8b\x54\x24\x14\xff\x35\xf4\x6f\xa4\x01\xe9\x78\xf7\x36\xff", 374);
			PatchBytes(0x167985A, (unsigned char*)"\x66\x83\x7d\x00\x00\x0f\x85\x30\xfe\xff\xff\xe9\x12\xfe\xff\xff", 16);
		}
		break;
	case DIGITAL_DX10:
		sAddresses::Pad_UpdateTimeStamps = 0x912620;
		sAddresses::Pad_ScaleStickValues = 0x912910;
		sAddresses::PadXenon_ctor = 0x8F5E30;
		sAddresses::PadProxyPC_AddPad = 0x8EB7F0;
		sAddresses::_addXenonJoy_Patch = 0x8F6609;
		sAddresses::_addXenonJoy_JumpOut = 0x8F6620;
		sAddresses::_PadProxyPC_Patch = 0x8EA190;
		sAddresses::_multisampling1 = 0x1064252;
		sAddresses::_multisampling2 = 0x1063F9D;
		sAddresses::_multisampling3 = 0x1063FA8;
		sAddresses::_descriptor_var = (uint32_t*)0x29A3710;
		ac_getNewDescriptor = (void*(__cdecl*)(uint32_t, uint32_t, uint32_t))0x903AB0;
		ac_allocate = (void* (__cdecl*)(int, uint32_t, void*, const void*, const char*, const char*, uint32_t, const char*))0x415BD0;
		ac_delete = (void(__cdecl*)(void*, void*, const char*))0x8F60D0;

		sAddresses::_ps3_controls[0] = 0x96D7A8 + 2;
		sAddresses::_ps3_controls[1] = 0x96D7C5 + 2;
		sAddresses::_ps3_controls[2] = 0x96D871 + 2;
		sAddresses::_ps3_controls[3] = 0x96D874 + 2;
		sAddresses::_ps3_controls_analog[0] = 0x96D83E + 4;
		sAddresses::_ps3_controls_analog[1] = 0x96D866 + 4;
		sAddresses::_ps3_controls_analog[2] = 0x96D88B + 4;
		sAddresses::_ps3_controls_analog[3] = 0x96D8A2 + 4;
		sAddresses::_skipIntroVideos = 0x4054B5;
		sAddresses::_maxNumNPCs_high1 = 0x584C6F;
		sAddresses::_maxNumNPCs_high2 = 0x1052320;
		sAddresses::_maxNumNPCs_low1 = 0x10522D1;
		sAddresses::_maxNumNPCs_low2 = 0x105123F;
		sAddresses::_increaseFOV = 0x5001A5;
		sAddresses::_increaseFOV_JumpOut = 0x5001AD;
		sAddresses::_smallObjectsCullDistanceModifier = 0x1052255;
		sAddresses::_smallObjectsCullDistanceModifier2 = 0x7BB58B;
		sAddresses::_smallObjectsCullDistanceModifier_JumpOut = 0x7BB590;
		sAddresses::_mediumObjectsCullDistanceModifier = 0x1052267;
		sAddresses::_mediumObjectsCullDistanceModifier2 = 0x7BB5B4;
		sAddresses::_mediumObjectsCullDistanceModifier_JumpOut = 0x7BB5B9;

		if (get_private_profile_bool("D3D10_RemoveDuplicateResolutions", TRUE))
		{
			// remove interlaced resolutions
			PatchByte(0x7BAD2E + 1, 0);
			PatchByte(0x7BAD70 + 1, 0);
			InjectHook(0x7F343D, &D3D10ResolutionContainer::GetDisplayModes_hook);
		}

		// WSGF Wide Screen Fix for DX10
		if (get_private_profile_bool("WideScreenFix", FALSE))
		{
			PatchBytes(0x403DEE, (unsigned char*)"\xdb\x44\xe4\x1c\xdc\x0d\x98\xa2\x6a\x01\xdb\x1d\xf8\xff\x9c\x02\x8b\x0d\xf8\xff\x9c\x02\xeb\x59", 24);
			PatchBytes(0x403E5D, (unsigned char*)"\xeb\x8f", 2);
			PatchBytes(0x7BDE88, (unsigned char*)"\xc7\x46\x60\x00\x00\x00\x7f", 7);
			PatchBytes(0x7C10F5, (unsigned char*)"\xeb\x1c", 2);
			PatchBytes(0x7C22A4, (unsigned char*)"\xe9\xe5\xe4\xeb\x00\x90\x90", 7);
			PatchBytes(0x7CAEC2, (unsigned char*)"\xe9\x0b\x59\xeb\x00\x90", 6);
			PatchBytes(0x8D883A, (unsigned char*)"\xe9\x5b\x7d\xda\x00\x90\x90\x90", 8);
			PatchBytes(0x168059A, (unsigned char*)"\x66\x81\x7b\x0c\x00\x05\x74\x0f\x0f\xb7\x4b\x0c\x0f\xb7\x53\x0e\xe9\x93\x82\x25\xff", 21);
			PatchBytes(0x16805B1, (unsigned char*)"\x81\xb8\x98\x00\x00\x00\x39\x8e\xe3\x3f\x0f\x8f\xc9\x00\x00\x00\xdf\x43\x0c\xd8\xb0\x98\x00\x00\x00\xdb\x54\xe4\x20\x8b\x54\xe4\x20\x0f\xb7\x4b\x0c\xdf\x43\x0e\xd8\xe9\xd8\x0d\x4a\xf7\x9f\x00\xd9\x1d\xf0\xff\x9c\x02\xdd\xd8\x66\x0f\xd6\x44\x24\x20\xf3\x0f\x7e\x80\x8c\x00\x00\x00\x8b\x44\x24\x14\x3b\xe8\x66\x0f\xd6\x44\x24\x28\xf3\x0f\x10\x44\x24\x28\xf2\x0f\x2a\xc9\x0f\x5a\xc0\xf2\x0f\x5e\xc1\xf3\x0f\x10\x4c\x24\x2c\x0f\x5a\xc9\xf2\x0f\x2a\xd2\xf2\x0f\x5e\xca\x66\x0f\x5a\xc0\x66\x0f\x5a\xc9\x0f\x84\x9d\x82\x25\xff\xeb\x06\x8d\x9b\x00\x00\x00\x00\xf3\x0f\x10\x5d\x00\x0f\x5a\xdb\xf3\x0f\x5a\xd0\xf2\x0f\x59\xd3\xf2\x0f\x5a\xd2\xf3\x0f\x11\x55\x00\xf3\x0f\x10\x55\x04\xf3\x0f\x58\x15\xf0\xff\x9c\x02\x0f\x5a\xd2\xf3\x0f\x5a\xd9\xf2\x0f\x59\xd3\x66\x0f\x5a\xd2\xf3\x0f\x11\x55\x04\x81\x4f\x6c\x00\x02\x00\x00\x83\xc5\x14\x3b\xe8\x75\xb8\xe9\x48\x82\x25\xff", 215);
			PatchBytes(0x168068A, (unsigned char*)"\xdf\x43\x0e\xd8\x88\x98\x00\x00\x00\xdb\x54\xe4\x20\x8b\x4c\xe4\x20\x0f\xb7\x53\x0e\xdf\x43\x0c\xd8\xe9\xd8\x0d\x4a\xf7\x9f\x00\xd9\x1d\xf0\xff\x9c\x02\xdd\xd8\x66\x0f\xd6\x44\x24\x20\xf3\x0f\x7e\x80\x8c\x00\x00\x00\x8b\x44\x24\x14\x3b\xe8\x66\x0f\xd6\x44\x24\x28\xf3\x0f\x10\x44\x24\x28\xf2\x0f\x2a\xc9\x0f\x5a\xc0\xf2\x0f\x5e\xc1\xf3\x0f\x10\x4c\x24\x2c\x0f\x5a\xc9\xf2\x0f\x2a\xd2\xf2\x0f\x5e\xca\x66\x0f\x5a\xc0\x66\x0f\x5a\xc9\x0f\x84\xd4\x81\x25\xff\xeb\x06\x8d\x9b\x00\x00\x00\x00\xeb\x4b\x90\x90\x90\xf3\x0f\x58\x1d\xf0\xff\x9c\x02\x0f\x5a\xdb\xf3\x0f\x5a\xd0\xf2\x0f\x59\xd3\xf2\x0f\x5a\xd2\xf3\x0f\x11\x55\x00\xf3\x0f\x10\x55\x04\x0f\x5a\xd2\xf3\x0f\x5a\xd9\xf2\x0f\x59\xd3\x66\x0f\x5a\xd2\xf3\x0f\x11\x55\x04\x81\x4f\x6c\x00\x02\x00\x00\x83\xc5\x14\x3b\xe8\x75\xb8\xe9\x7f\x81\x25\xff\x81\x7d\x00\x00\x00\xba\xc3\x74\x2b\x81\x7d\x00\x00\x00\xa8\xc3\x74\x22\x81\x7d\x00\x00\x00\xc2\xc2\x7f\x07\xf3\x0f\x10\x5d\x00\xeb\x96\x81\x7d\x00\x00\x00\x0e\xc3\x7c\x02\xeb\xee\xe9\xc7\x01\x00\x00", 249);
			PatchBytes(0x1680785, (unsigned char*)"\xf3\x0f\x10\x5d\x00\xeb\x85", 7);
			PatchBytes(0x168078E, (unsigned char*)"\x81\x7c\xe4\x0c\x39\x8e\xe3\x3f\x7c\x0e\xd9\x43\x40\xd9\x5c\x24\x08\xe9\x07\x1b\x14\xff", 22);
			PatchBytes(0x16807A6, (unsigned char*)"\xd9\x43\x40\xd8\x0d\x4a\xf7\x9f\x00\xd9\xf2\xdd\xd8\xdc\x0d\x98\xa2\x6a\x01\xd8\x74\xe4\x0c\xd9\xe8\xd9\xf3\xd8\x35\x4a\xf7\x9f\x00\xd9\x5c\xe4\x08\xe9\xdb\x1a\x14\xff", 42);
			PatchBytes(0x16807D2, (unsigned char*)"\x81\xb9\x98\x00\x00\x00\x39\x8e\xe3\x3f\x0f\x8c\xcb\x00\x00\x00\xd9\x81\x90\x00\x00\x00\xd9\x7c\x24\x0a\x0f\xb7\x44\x24\x0a\x0d\x00\x0c\x00\x00\x89\x44\x24\x0c\xd9\x6c\x24\x0c\xdf\x7c\x24\x0c\x8b\x54\x24\x0c\x52\xd9\x6c\x24\x0e\xd9\x81\x8c\x00\x00\x00\xd9\x7c\x24\x0e\x0f\xb7\x44\x24\x0e\x0d\x00\x0c\x00\x00\x89\x44\x24\x10\xd9\x6c\x24\x10\xdf\x7c\x24\x10\x8b\x44\x24\x10\x50\xdb\x44\xe4\x04\xdc\x35\xa0\xa2\x6a\x01\xdb\x14\xe4\xd8\xa9\x8c\x00\x00\x00\xd8\x0d\x4a\xf7\x9f\x00\xdb\x1d\xf4\xff\x9c\x02\xd9\x6c\x24\x12\xd9\x81\x88\x00\x00\x00\xd9\x7c\x24\x12\x0f\xb7\x44\x24\x12\x0d\x00\x0c\x00\x00\x89\x44\x24\x14\xd9\x6c\x24\x14\xdf\x7c\x24\x14\x8b\x54\x24\x14\x52\xd9\x6c\x24\x16\xd9\x81\x84\x00\x00\x00\xd9\x7c\x24\x16\x8b\xcf\x0f\xb7\x44\x24\x16\x0d\x00\x0c\x00\x00\x89\x44\x24\x18\xd9\x6c\x24\x18\xdf\x7c\x24\x18\x8b\x44\x24\x18\xff\x35\xf4\xff\x9c\x02\xe9\xb7\xa6\x14\xff\xd9\x81\x8c\x00\x00\x00\xdc\x0d\xa0\xa2\x6a\x01\xd9\xc0\xd8\xa9\x90\x00\x00\x00\xd8\x0d\x4a\xf7\x9f\x00\xdb\x1d\xf4\xff\x9c\x02\xd9\x7c\x24\x0a\x0f\xb7\x44\x24\x0a\x0d\x00\x0c\x00\x00\x89\x44\x24\x0c\xd9\x6c\x24\x0c\xdf\x7c\x24\x0c\x8b\x54\x24\x0c\x52\xd9\x6c\x24\x0e\xd9\x81\x8c\x00\x00\x00\xd9\x7c\x24\x0e\x0f\xb7\x44\x24\x0e\x0d\x00\x0c\x00\x00\x89\x44\x24\x10\xd9\x6c\x24\x10\xdf\x7c\x24\x10\x8b\x44\x24\x10\x50\xd9\x6c\x24\x12\xd9\x81\x88\x00\x00\x00\xd9\x7c\x24\x12\x0f\xb7\x44\x24\x12\x0d\x00\x0c\x00\x00\x89\x44\x24\x14\xd9\x6c\x24\x14\xdf\x7c\x24\x14\x8b\x54\x24\x14\xff\x35\xf4\xff\x9c\x02\xe9\xf1\xa5\x14\xff", 374);
			PatchBytes(0x168094A, (unsigned char*)"\x66\x83\x7d\x00\x00\x0f\x85\x30\xfe\xff\xff\xe9\x12\xfe\xff\xff", 16);
		}
		break;
	default:
		return;
	}

	patch();

}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (MEMCMP32(0x00401375 + 1, 0x42d6)) // dx9
		{
			InitAddresses(DIGITAL_DX9);
		}
		else if (MEMCMP32(0x004013DE + 1, 0x428d)) // dx10
		{
			InitAddresses(DIGITAL_DX10);
		}
	}

	return TRUE;
}
