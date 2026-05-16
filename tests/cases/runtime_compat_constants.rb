puts RUBY_ENGINE
puts RUBY_VERSION
puts RUBY_PLATFORM
puts RUBY_DESCRIPTION
puts RUBY_ENGINE_VERSION
puts RUBY_PATCHLEVEL
puts RUBY_REVISION
puts RUBY_RELEASE_DATE
puts RUBY_COPYRIGHT

puts Marshal::MAJOR_VERSION
puts Marshal::MINOR_VERSION

module RuntimeCompatConstants
  VALUE = 7

  puts private_constant(:VALUE).nil?
  puts public_constant(:VALUE).nil?
  puts deprecate_constant(:VALUE).nil?
  puts VALUE
end

puts Object.private_constant(:Marshal).nil?
puts Object.public_constant(:Marshal).nil?
puts Object.deprecate_constant(:Marshal).nil?
puts Marshal::MAJOR_VERSION
