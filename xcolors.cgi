#!/usr/bin/env ruby
# coding: utf-8
# -*-ruby-*-

RGBS=[ "/usr/X11R6/share/X11/rgb.txt",
       "/usr/share/X11/rgb.txt" ]

RGBS.each { |path|
  if File.exists? path
    RGB=path
    break
  end
}

def rgb_each()
  f = File.open(RGB, "r")
  p = /([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+(.*)[ \t]*$/

  f.each { |ln|
    m = p.match(ln)

    if m != nil
      spec = "#%02X%02X%02X" % [m[1].to_i, m[2].to_i, m[3].to_i]
      yield spec, m[4]
    end
  }
end

header = <<END
<html>
<style>
div { padding: 1ex; font-family: "Monaco", monospace; }
table { margin-left: auto; margin-right: auto; }
.black { color: black }
.white { color: white }
</style>
<body>
<h1>X11 color names</h1>
<p>This is the color names in #{RGB}.</p>

<table>
END

footer = <<END
</table>
</body>
</html>
END

if ENV["GATEWAY_INTERFACE"]
  puts "Content-type: text/html\n\n"
end

puts header
rgb_each { |spec, name|
  puts "<tr>"
  puts "<td><div class=\"white\" style=\"background-color: #{spec}\">"
  puts " #{spec} </div>"
  puts "<td><div class=\"black\" style=\"background-color: #{spec}\">"
  puts " #{spec} </div>"
  puts "<td><div style=\"background-color: white; color: #{spec}\">"
  puts " #{name} </div></td>"
  puts "<td><div style=\"background-color: black; color: #{spec}\">"
  puts " #{name} </div></td>"
  puts "</tr>"
}
puts footer
