/**
 * Sessions & Local Multiplayer Handlers (Phase 22)
 *
 * Complete session management including:
 * - Session Management (local session settings, session interface)
 * - Local Multiplayer (split-screen, local players)
 * - LAN (LAN play configuration, hosting, joining)
 * - Voice Chat (voice settings, channels, muting, attenuation, push-to-talk)
 *
 * @module sessions-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { createSubActionDispatcher } from './common-handlers.js';


/**
 * Handles all sessions and local multiplayer actions for the manage_sessions tool.
 */
export async function handleSessionsTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const { sendRequest } = createSubActionDispatcher(tools, args, {
    toolName: 'manage_sessions',
    domainName: 'sessions'
  });

  switch (action) {
    // ========================================================================
    // Session Management (2 actions)
    // ========================================================================
    case 'configure_local_session_settings':
      return sendRequest('configure_local_session_settings');

    case 'configure_session_interface':
      return sendRequest('configure_session_interface');

    // ========================================================================
    // Local Multiplayer (4 actions)
    // ========================================================================
    case 'configure_split_screen':
      return sendRequest('configure_split_screen');

    case 'set_split_screen_type':
      return sendRequest('set_split_screen_type');

    case 'add_local_player':
      return sendRequest('add_local_player');

    case 'remove_local_player':
      return sendRequest('remove_local_player');

    // ========================================================================
    // LAN (3 actions)
    // ========================================================================
    case 'configure_lan_play':
      return sendRequest('configure_lan_play');

    case 'host_lan_server':
      return sendRequest('host_lan_server');

    case 'join_lan_server':
      return sendRequest('join_lan_server');

    // ========================================================================
    // Voice Chat (6 actions)
    // ========================================================================
    case 'enable_voice_chat':
      return sendRequest('enable_voice_chat');

    case 'configure_voice_settings':
      return sendRequest('configure_voice_settings');

    case 'set_voice_channel':
      return sendRequest('set_voice_channel');

    case 'mute_player':
      return sendRequest('mute_player');

    case 'set_voice_attenuation':
      return sendRequest('set_voice_attenuation');

    case 'configure_push_to_talk':
      return sendRequest('configure_push_to_talk');

    // ========================================================================
    // Utility (1 action)
    // ========================================================================
    case 'get_sessions_info':
      return sendRequest('get_sessions_info');

    case 'get_online_capabilities':
      return sendRequest('get_online_capabilities');
    case 'get_online_session_status':
      return sendRequest('get_online_session_status');
    case 'get_online_identity_status':
      return sendRequest('get_online_identity_status');
    case 'get_online_presence':
      return sendRequest('get_online_presence');
    case 'set_online_presence':
      return sendRequest('set_online_presence');
    case 'get_online_friends':
      return sendRequest('get_online_friends');
    case 'send_online_friend_invite':
      return sendRequest('send_online_friend_invite');
    case 'accept_online_friend_invite':
      return sendRequest('accept_online_friend_invite');
    case 'configure_network_conditions':
      return sendRequest('configure_network_conditions');
    case 'create_online_session':
      return sendRequest('create_online_session');
    case 'find_online_sessions':
      return sendRequest('find_online_sessions');
    case 'join_online_session':
      return sendRequest('join_online_session');
    case 'destroy_online_session':
      return sendRequest('destroy_online_session');

    default:
      return {
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown sessions action: ${action}`
      };
  }
}
