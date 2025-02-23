__update_table = {
	"scpt/update.lua",
	"scpt/c-init.lua",
	"scpt/player.lua",
	"scpt/spells.lua",
	"scpt/s_aux.lua",
	"scpt/s_mana.lua",
	"scpt/s_fire.lua",
	"scpt/s_air.lua",
	"scpt/s_water.lua",
	"scpt/s_earth.lua",
	"scpt/s_convey.lua",
	"scpt/s_divin.lua",
	"scpt/s_tempo.lua",
--	"scpt/s_meta.lua",
	"scpt/s_nature.lua",
--	"scpt/s_mind.lua",
	"scpt/s_udun.lua",

	"scpt/p_offense.lua",
	"scpt/p_defense.lua",
	"scpt/p_curing.lua",
	"scpt/p_support.lua",

	"scpt/dr_arcane.lua",
	"scpt/dr_physical.lua",

	"scpt/d_astral.lua",

	"scpt/m_ppower.lua",
	"scpt/m_tcontact.lua",
	"scpt/m_mintrusion.lua",

	"scpt/meta.lua",
	"scpt/xml.lua",

	"scpt/audio.lua",
	"scpt/guide.lua",

	"scpt/classes.lua",
	"scpt/races.lua",
	"scpt/traits.lua",
	"scpt/bpr.lua",
	"scpt/mimicry.lua",
	"scpt/runecraft.lua",
	"scpt/activations.lua",

	"scpt/o_shadow.lua",
	"scpt/o_spirit.lua",
	"scpt/o_hereticism.lua",
	"scpt/o_unlife.lua",
}

function update_client()
	local k, e

	for k, e in __update_table do
		remote_update_lua(Ind, e)
	end
end
