
posix = require'posix'

-- -- https://man7.org/linux/man-pages/man3/system.3.html
-- print(posix.exec('/bin/sh', {'-c', 'echo foo bar $PATH', nil}))


shix = require'shix'
shix.sleep(0.5)
