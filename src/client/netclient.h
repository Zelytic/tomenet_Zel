#define MIN_RECEIVE_WINDOW_SIZE		1
#define MAX_RECEIVE_WINDOW_SIZE		4

extern int	receive_window_size;

static int Net_packet();
int Net_setup(void);
int Net_verify(char *real, char *nick, char *pass);
int Net_init(int fd);
void Net_cleanup(void);
int Net_flush(void);
int Net_fd(void);
int Net_start(int sex, int race, int class);
int Net_input(void);
int Receive_end(void);
int Receive_quit(void);
void Receive_login(void);
int Receive_file(void);
int Receive_reply(int *replyto, int *result);
int Receive_magic(void);
int Receive_stat(void);
int Receive_hp(void);
int Receive_stamina(void);
int Receive_ac(void);
int Receive_inven(void);
int Receive_inven_wide(void);
int Receive_equip(void);
int Receive_char_info(void);
int Receive_various(void);
int Receive_mimic(void);
int Receive_plusses(void);
int Receive_experience(void);
int Receive_skill_init(void);
int Receive_skill_points(void);
int Receive_skill_info(void);
int Receive_gold(void);
int Receive_sp(void);
int Receive_history(void);
int Receive_char(void);
int Receive_message(void);
int Receive_state(void);
int Receive_title(void);
int Receive_depth(void);
int Receive_confused(void);
int Receive_poison(void);
int Receive_study(void);
int Receive_food(void);
int Receive_fear(void);
int Receive_speed(void);
int Receive_cut(void);
int Receive_blind(void);
int Receive_stun(void);
int Receive_item(void);
int Receive_spell_info(void);
int Receive_technique_info(void);
int Receive_direction(void);
int Receive_flush(void);
int Receive_line_info(void);
int Receive_floor(void);
int Receive_special_other(void);
int Receive_store(void);
int Receive_store_wide(void);
int Receive_store_info(void);
int Receive_store_kick(void);
int Receive_store_action(void);
int Receive_sell(void);
int Receive_target_info(void);
int Receive_sound(void);
int Receive_mini_map(void);
int Receive_special_line(void);
int Receive_floor(void);
int Receive_pickup_check(void);
int Receive_party(void);
int Receive_party_stats(void);
int Receive_skills(void);
int Receive_pause(void);
int Receive_monster_health(void);
int Receive_sanity(void);
int Receive_chardump(void);
int Receive_beep(void);
int Receive_AFK(void);
int Receive_encumberment(void);
int Receive_keepalive(void);
int Receive_ping(void);
int Receive_extra_status(void);
int Receive_unique_monster(void);
int Receive_weather(void);
int Receive_inventory_revision(void);
int Receive_account_info(void);
