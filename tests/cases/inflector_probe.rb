# dry-inflector-style probe: Regexp-backed string transformation with rule registry
class Inflections
  def initialize
    @plurals    = []
    @singulars  = []
    @irregulars = {}
  end

  def plural(rule, replacement)
    @plurals.unshift([rule, replacement])
  end

  def singular(rule, replacement)
    @singulars.unshift([rule, replacement])
  end

  def irregular(single, plural)
    @irregulars[single] = plural
    @irregulars[plural]  = single
  end

  def pluralize(word)
    return @irregulars[word] if @irregulars.key?(word)
    @plurals.each { |rule, repl| return word.gsub(rule, repl) if word =~ rule }
    word
  end

  def singularize(word)
    return @irregulars[word] if @irregulars.key?(word)
    @singulars.each { |rule, repl| return word.gsub(rule, repl) if word =~ rule }
    word
  end
end

class Inflector
  def initialize
    i = @inflections = Inflections.new
    # plural rules — general first, specific last (unshift promotes specifics to front)
    i.plural(/s?$/i,                   's')
    i.plural(/(x|ch|ss|sh|quiz)$/i,    '\1es')
    i.plural(/([^aeio])y$/i,           '\1ies')
    # singular rules
    i.singular(/s$/i,                  '')
    i.singular(/(x|ch|ss|sh)es$/i,     '\1')
    i.singular(/([^aeio])ies$/i,       '\1y')
    # irregulars
    i.irregular("person", "people")
    i.irregular("child",  "children")
    i.irregular("ox",     "oxen")
  end

  def camelize(str)
    str.to_s.gsub(/(?:^|_+)([a-z])/) { $1.upcase }
  end

  def underscore(str)
    str.to_s
       .gsub(/([A-Z]+)([A-Z][a-z])/, '\1_\2')
       .gsub(/([a-z\d])([A-Z])/, '\1_\2')
       .downcase
  end

  def dasherize(str)
    underscore(str).gsub('_', '-')
  end

  def humanize(str)
    str.to_s.gsub(/_/, ' ').gsub(/\b([a-z])/) { $1.upcase }
  end

  def pluralize(word)    = @inflections.pluralize(word)
  def singularize(word)  = @inflections.singularize(word)
end

i = Inflector.new

puts i.camelize("ruby_on_rails")
puts i.camelize("hello_world_foo")
puts i.underscore("RubyOnRails")
puts i.underscore("HTMLParser")
puts i.dasherize("ruby_on_rails")
puts i.humanize("first_name")
puts i.pluralize("person")
puts i.pluralize("child")
puts i.pluralize("ox")
puts i.pluralize("category")
puts i.pluralize("box")
puts i.pluralize("dog")
puts i.singularize("people")
puts i.singularize("children")
puts i.singularize("categories")
puts i.singularize("boxes")
puts i.singularize("dogs")
