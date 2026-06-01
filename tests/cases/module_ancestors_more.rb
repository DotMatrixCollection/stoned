module MixinA; end
module MixinB; end

class AncestorBox
  include MixinA
  prepend MixinB
end

p AncestorBox.ancestors[0, 3].map(&:to_s)
p AncestorBox.included_modules.map(&:to_s).include?("MixinA")
