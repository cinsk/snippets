#!/usr/bin/env ruby
# -*-ruby-*-

RGB="/usr/X11R6/share/X11/rgb.txt"


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
body { font-family: "Monaco"; }
div { padding: 1ex }
.black { color: black }
.white { color: white }
</style>
<body>
<table>
END

footer = <<END
</table>
</body>
</html>
END

puts "Content-type: text/html\n"

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
