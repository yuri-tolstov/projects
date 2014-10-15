require( "iuplua" )

-- Creates a text, sets its value and turns on text readonly mode
text = iup.text{readonly = "YES", value = "Show or hide this text"}

item_start = iup.item{title = "Start"}
item_stop = iup.item{title = "Stop"}
item_read = iup.item{title = "Read"}
item_exit = iup.item{title = "Exit"}

function item_start:action()
  text.visible = "YES"
  return iup.DEFAULT
end

function item_stop:action()
  text.visible = "YES"
  return iup.DEFAULT
end

function item_read:action()
  text.visible = "NO"
  return iup.DEFAULT
end

function item_exit:action()
  return iup.CLOSE
end

menu = iup.menu{item_start, item_stop, item_read, item_exit}

-- Creates dialog with a text, sets its title and associates a menu to it
-- dialog = iup.dialog{text; title="Menu Example", menu=menu}
dialog = iup.dialog{text; title = "Call Capture Tool", menu = menu, size = "QUARTERxQUARTER"}

-- Shows dialog in the center of the screen
-- dialog:showxy(iup.CENTER, iup.CENTER)
dialog:show()
iup.MainLoop()

