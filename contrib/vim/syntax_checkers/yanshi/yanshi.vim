if exists('g:loaded_syntastic_yanshi_yanshi_checker')
  finish
endif
let g:loaded_syntastic_yanshi_yanshi_checker = 1

let s:save_cpo = &cpo
set cpo&vim

fu! SyntaxCheckers_yanshi_yanshi_GetLocList() dict
  let makeprg = self.makeprgBuild({ 'args': '-c' })

  let errorformat =
        \ '%C  %.%#,'   .
        \ '%E%f %l%*[^:]:%c%*[-0-9] %m'

  return SyntasticMake({
        \ 'makeprg': makeprg,
        \ 'errorformat': errorformat })
endf

call g:SyntasticRegistry.CreateAndRegisterChecker({
      \ 'filetype': 'yanshi',
      \ 'name': 'yanshi'})

let &cpo = s:save_cpo
unlet s:save_cpo
