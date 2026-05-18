require "bundler"

module Bundler
  class Injector
    def self.inject(contents, gems)
      base = contents.to_s
      additions = Array(gems).map { |gem| gem_declaration(gem) }.compact
      additions.reject! do |line|
        gem_name = extract_name(line)
        gem_name && gem_already_present?(base, gem_name)
      end
      base = base.chomp
      ([base] + additions).reject { |line| line.nil? || line.empty? }.join("\n") + "\n"
    end

    def self.gem_declaration(gem)
      if gem.is_a?(Hash)
        name = gem[:name] || gem["name"]
        return nil unless name

        line = "gem #{name.to_s.inspect}"
        requirements = Array(gem[:version] || gem["version"] || gem[:requirement] || gem["requirement"]).compact
        requirements.each { |req| line << ", #{req.to_s.inspect}" }

        options = []
        append_option(options, "group", gem[:group] || gem["group"])
        append_option(options, "groups", gem[:groups] || gem["groups"])
        append_option(options, "path", gem[:path] || gem["path"])
        append_option(options, "git", gem[:git] || gem["git"])
        append_option(options, "branch", gem[:branch] || gem["branch"])
        if gem.key?(:require) || gem.key?("require")
          require_value = gem.key?(:require) ? gem[:require] : gem["require"]
          append_option(options, "require", require_value)
        end
        line << ", #{options.join(', ')}" unless options.empty?
        line
      else
        "gem #{gem.inspect}"
      end
    end

    def self.extract_name(declaration)
      match = declaration.match(/^gem\s+["']([^"']+)["']/)
      match && match[1]
    end

    def self.gem_already_present?(contents, name)
      contents.each_line do |line|
        stripped = line.strip
        return true if stripped.start_with?("gem #{name.to_s.inspect}")
      end
      false
    end

    def self.append_option(options, key, value)
      return if value.nil?
      formatted =
        if value.is_a?(Array)
          "[" + value.map { |entry| entry.to_s.inspect }.join(", ") + "]"
        elsif value == true || value == false
          value ? "true" : "false"
        else
          value.to_s.inspect
        end
      options << "#{key}: #{formatted}"
    end

    private_class_method :gem_declaration, :extract_name, :gem_already_present?, :append_option
  end
end
