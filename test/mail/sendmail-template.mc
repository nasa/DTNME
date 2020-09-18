divert(-1)dnl
dnl #
dnl # This is the sendmail macro config file for m4. If you make changes to
dnl # /etc/mail/sendmail.mc, you will need to regenerate the
dnl # /etc/mail/sendmail.cf file by confirming that the sendmail-cf package is
dnl # installed and then performing a
dnl #
dnl #     make -C /etc/mail
dnl #
include(`/usr/share/sendmail-cf/m4/cf.m4')dnl
VERSIONID(`setup for Red Hat Linux')dnl
OSTYPE(`linux')dnl
define(`SMART_HOST',`__SMART_HOST__')

define(`confDEF_USER_ID',``8:12'')dnl
define(`confTRUSTED_USER', `smmsp')dnl
dnl define(`confAUTO_REBUILD')dnl
define(`confTO_CONNECT', `1m')dnl
define(`confTRY_NULL_MX_LIST',true)dnl
define(`PROCMAIL_MAILER_PATH',`/usr/bin/procmail')dnl
define(`ALIAS_FILE', `/etc/aliases')dnl
dnl define(`STATUS_FILE', `/etc/mail/statistics')dnl
dnl define(`UUCP_MAILER_MAX', `2000000')dnl
define(`confUSERDB_SPEC', `/etc/mail/userdb.db')dnl
define(`confPRIVACY_FLAGS', `authwarnings,novrfy,noexpn,restrictqrun')dnl
define(`confAUTH_OPTIONS', `A')dnl

undefine(`confBIND_OPTS')dnl
define(`confSERVICE_SWITCH_FILE', `/etc/mail/service.switch-nodns')dnl
define(`confDONT_PROBE_INTERFACES', `True')dnl
define(`confMCI_CACHE_SIZE',`__CACHE_SIZE__')dnl
define(`confDELIVERY_MODE',`__DELIVERY_MODE__')dnl
define(`confSINGLE_THREAD_DELIVERY',__SINGLE_THREAD__)dnl
define(`confHOST_STATUS_DIRECTORY',`__HOST_STATUS_DIRECTORY__')dnl
define(`QUEUE_DIR',`/var/spool/__QUEUE_DIR__')dnl
define(`confTO_IDENT', `0')dnl
dnl FEATURE(delay_checks)dnl
define(`confTO_HOSTSTATUS',`__HOSTSTATUS_TIMEOUT__')dnl


FEATURE(`no_default_msa',`dnl')dnl
FEATURE(`smrsh',`/usr/sbin/smrsh')dnl
FEATURE(`mailertable',`hash -o /etc/mail/mailertable.db')dnl
FEATURE(`virtusertable',`hash -o /etc/mail/virtusertable.db')dnl
dnl FEATURE(redirect)dnl
dnl FEATURE(always_add_domain)dnl
FEATURE(use_cw_file)dnl
FEATURE(use_ct_file)dnl
FEATURE(`nocanonify')dnl
FEATURE(`masquerade_envelope')dnl
FEATURE(`promiscuous_relay')

FEATURE(`nouucp', `reject')dnl
FEATURE(local_procmail,`',`procmail -t -Y -a $h -d $u')dnl
FEATURE(`access_db',`hash -T<TMPF> -o /etc/mail/access.db')dnl
FEATURE(`blacklist_recipients')dnl
EXPOSED_USER(`root')dnl
FEATURE(`accept_unresolvable_domains')dnl
dnl FEATURE(`relay_based_on_MX')dnl
LOCAL_DOMAIN(`__LOCAL_DOMAIN__')dnl
MAILER(local)dnl
MAILER(smtp)dnl
MAILER(procmail)dnl

