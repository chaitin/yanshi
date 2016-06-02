if exists('current_compiler')
  finish
endif
let current_compiler = 'yanshi'

if exists(':CompilerSet') != 2
  command -nargs=* CompilerSet setlocal <args>
endif
CompilerSet errorformat=
      \%C\ \ %.%#,
      \%E%f\ %l%*[^:]:%c%*[-0-9]\ %m,
CompilerSet makeprg=yanshi\ $*\ %
